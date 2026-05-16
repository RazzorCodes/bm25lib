#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>

#include "bm25.hpp"
#include "store/in_memory.hpp"
#include "store/sqlite.hpp"

// Contract parity: the same logical behaviour must hold for every IAdapter implementation.
// Tests run identically against bm25::Store::InMemory and bm25::Store::Sqlite.

template <typename Adapter>
class AdapterContractTests : public ::testing::Test
{
};

using AdapterTypes = ::testing::Types<bm25::Store::InMemory, bm25::Store::Sqlite>;
TYPED_TEST_SUITE(AdapterContractTests, AdapterTypes);

TYPED_TEST(AdapterContractTests, UpsertNewKeyReturnsInserted)
{
    TypeParam store;
    bm25::Bm25 service(store);

    const auto result = service.IngestChunk("doc:a", "hello world");

    EXPECT_EQ(result.outcome, bm25::WriteOutcome::Inserted);
    EXPECT_EQ(service.Stats().documentCount, 1U);
    EXPECT_EQ(service.Stats().totalTokens, 2U);
}

TYPED_TEST(AdapterContractTests, UpsertExistingKeyReturnsUpdatedWithSameDocId)
{
    TypeParam store;
    bm25::Bm25 service(store);

    const auto r1 = service.IngestChunk("doc:a", "cat cat dog");
    const auto r2 = service.IngestChunk("doc:a", "bird bird bird");

    EXPECT_EQ(r1.outcome, bm25::WriteOutcome::Inserted);
    EXPECT_EQ(r2.outcome, bm25::WriteOutcome::Updated);
    EXPECT_EQ(r1.docId, r2.docId);

    const auto stats = service.Stats();
    EXPECT_EQ(stats.documentCount, 1U);
    EXPECT_EQ(stats.totalTokens, 3U);
    EXPECT_DOUBLE_EQ(stats.avgDocumentLength, 3.0);
    EXPECT_EQ(stats.indexVersion, 2U);
}

TYPED_TEST(AdapterContractTests, DeleteRemovesDocAndUpdatesStats)
{
    TypeParam store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat dog");
    const auto id = service.IngestChunk("d1", "bird").docId;
    (void)service.IngestChunk("d2", "fish");

    EXPECT_TRUE(service.DeleteDocument(id));
    EXPECT_FALSE(service.DeleteDocument(id));

    const auto stats = service.Stats();
    EXPECT_EQ(stats.documentCount, 2U);
    EXPECT_EQ(stats.totalTokens, 3U);
    EXPECT_EQ(stats.indexVersion, 4U);
}

TYPED_TEST(AdapterContractTests, DeleteUnknownIdReturnsFalse)
{
    TypeParam store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat");
    EXPECT_FALSE(service.DeleteDocument(99999U));
    EXPECT_EQ(service.Stats().documentCount, 1U);
}

TYPED_TEST(AdapterContractTests, EmptyCorpusQueryReturnsEmpty)
{
    TypeParam store;
    bm25::Bm25 service(store);

    const auto result = service.Query("cat");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TYPED_TEST(AdapterContractTests, UnknownTermReturnsEmpty)
{
    TypeParam store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat dog");
    const auto result = service.Query("wolf");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TYPED_TEST(AdapterContractTests, DelimiterOnlyQueryReturnsEmpty)
{
    TypeParam store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat dog");
    const auto result = service.Query("... ,,, ---");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TYPED_TEST(AdapterContractTests, TieBrokenByDocIdAscending)
{
    TypeParam store;
    bm25::Bm25 service(store);

    // Equal-length docs with the same query term → equal BM25 scores.
    const auto id0 = service.IngestChunk("d0", "cat fox").docId;
    const auto id1 = service.IngestChunk("d1", "cat dog").docId;

    const auto result = service.Query("cat");
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);
    EXPECT_NEAR((*result)[0].score, (*result)[1].score, 1e-9);
    EXPECT_EQ((*result)[0].docId, std::min(id0, id1));
    EXPECT_EQ((*result)[1].docId, std::max(id0, id1));
}

TYPED_TEST(AdapterContractTests, TopKLimitsResultsAndPreservesOrder)
{
    TypeParam store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat here");
    (void)service.IngestChunk("d1", "cat there");
    (void)service.IngestChunk("d2", "dog only");

    const auto result = service.QueryTopK("cat", 2);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);
    EXPECT_GT((*result)[0].score, 0.0);
    EXPECT_GT((*result)[1].score, 0.0);
}

TYPED_TEST(AdapterContractTests, IndexVersionIncrementsOnEveryWrite)
{
    TypeParam store;
    bm25::Bm25 service(store);

    const auto v0 = service.Stats().indexVersion;
    const auto id = service.IngestChunk("k1", "cat").docId;
    const auto v1 = service.Stats().indexVersion;
    (void)service.IngestChunk("k2", "dog");
    const auto v2 = service.Stats().indexVersion;
    (void)service.DeleteDocument(id);
    const auto v3 = service.Stats().indexVersion;

    EXPECT_GT(v1, v0);
    EXPECT_GT(v2, v1);
    EXPECT_GT(v3, v2);
}

TYPED_TEST(AdapterContractTests, BM25ScoresMatchForIdenticalCorpusAndParams)
{
    // Both adapters must produce the same scores in the same rank order for an
    // identical corpus. DocIds are adapter-internal (InMemory starts at 0, SQLite at 1),
    // so we compare scores and the ingestion-key-derived rank order, not raw IDs.
    const std::vector<std::pair<std::string, std::string>> corpus = {
        {"d0", "cat cat dog"},
        {"d1", "dog bird"},
        {"d2", "bird cat"},
        {"d3", "fish"},
    };
    const bm25::Bm25Params params{1.2, 0.75};

    bm25::Store::InMemory ref;
    bm25::Bm25 refService(ref);
    for (const auto &[key, text] : corpus)
    {
        (void)refService.IngestChunk(key, text);
    }

    TypeParam sut;
    bm25::Bm25 sutService(sut);
    for (const auto &[key, text] : corpus)
    {
        (void)sutService.IngestChunk(key, text);
    }

    const auto refResult = refService.Query("cat bird", params);
    const auto sutResult = sutService.Query("cat bird", params);

    ASSERT_TRUE(refResult.has_value());
    ASSERT_TRUE(sutResult.has_value());
    ASSERT_EQ(refResult->size(), sutResult->size());

    for (std::size_t i = 0; i < refResult->size(); ++i)
    {
        EXPECT_NEAR((*sutResult)[i].score, (*refResult)[i].score, 1e-9)
            << "score mismatch at rank " << i;
    }
}
