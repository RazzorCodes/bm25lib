#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "bm25.hpp"
#include "store/in_memory.hpp"

// Helper: find score for a specific docId in ranked results (0.0 if absent).
static double scoreFor(const bm25::RankedResults &results, const bm25::DocumentId id)
{
    const auto it =
        std::find_if(results.begin(), results.end(),
                     [id](const bm25::RankedResult &r) { return r.docId == id; });
    return it != results.end() ? it->score : 0.0;
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkCountsTotalTokensAndPerTermFrequency /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("The willy quick fox doesn't jump over the lazy brown dog.");

    const auto stats = store.Stats();
    ASSERT_EQ(stats.documentCount, 1U);

    const auto postings = store.FetchPostings({"the"});
    ASSERT_EQ(postings.size(), 1U);
    EXPECT_EQ(postings[0].second.tokenCount, 11U);
    EXPECT_EQ(postings[0].second.termFrequencies.size(), 10U);
    EXPECT_EQ(postings[0].second.termFrequencies.at("the"), 2U);
    EXPECT_EQ(postings[0].second.termFrequencies.at("doesn't"), 1U);
    EXPECT_EQ(postings[0].second.termFrequencies.at("dog"), 1U);
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkMergesRepeatedTokensAfterNormalization /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("Go go GO!");

    const auto postings = store.FetchPostings({"go"});
    ASSERT_EQ(postings.size(), 1U);
    EXPECT_EQ(postings[0].second.tokenCount, 3U);
    ASSERT_EQ(postings[0].second.termFrequencies.size(), 1U);
    EXPECT_EQ(postings[0].second.termFrequencies.at("go"), 3U);
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkHandlesDelimiterOnlyInput /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("... ,,, ---");

    const auto stats = store.Stats();
    ASSERT_EQ(stats.documentCount, 1U);

    // Empty-token docs are in the store but match no term query.
    const auto postings = store.FetchPostings({"anything"});
    EXPECT_TRUE(postings.empty());
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkIsIdempotentBySameContent /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto id1 = service.IngestChunk("hello world");
    const auto id2 = service.IngestChunk("hello world");

    EXPECT_EQ(id1, id2);
    EXPECT_EQ(store.Stats().documentCount, 1U);
    EXPECT_EQ(store.Stats().totalTokens, 2U);
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkTracksDocumentFrequencies /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("cat cat dog");
    (void)service.IngestChunk("dog bird");
    (void)service.IngestChunk("... ,,, ---");
    (void)service.IngestChunk("bird CAT");

    const auto freqs = store.DocumentFrequencies();
    ASSERT_EQ(freqs.size(), 3U);
    EXPECT_EQ(freqs.at("cat"), 2U);
    EXPECT_EQ(freqs.at("dog"), 2U);
    EXPECT_EQ(freqs.at("bird"), 2U);

    const auto stats = store.Stats();
    EXPECT_DOUBLE_EQ(stats.avgDocumentLength, 1.75);
}

TEST(Bm25ServiceTests /*unused*/, DeleteDocumentRemovesItFromStoreAndStats /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("cat cat dog");
    const auto idDog = service.IngestChunk("dog bird");
    (void)service.IngestChunk("bird CAT");

    ASSERT_EQ(store.Stats().documentCount, 3U);
    service.DeleteDocument(idDog);

    EXPECT_EQ(store.Stats().documentCount, 2U);
    const auto freqs = store.DocumentFrequencies();
    EXPECT_EQ(freqs.at("dog"), 1U);  // only in first doc now
    EXPECT_EQ(freqs.at("bird"), 1U); // only in third doc now
}

TEST(Bm25ServiceTests /*unused*/, DeleteUnknownDocumentIdIsNoOp /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("cat");
    ASSERT_EQ(store.Stats().documentCount, 1U);

    EXPECT_NO_THROW(service.DeleteDocument(99999U));
    EXPECT_EQ(store.Stats().documentCount, 1U);
}

TEST(Bm25ServiceTests /*unused*/, IndexVersionIncrementsOnEveryWrite /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto v0 = store.Stats().indexVersion;
    const auto id = service.IngestChunk("cat");
    const auto v1 = store.Stats().indexVersion;
    (void)service.IngestChunk("dog");
    const auto v2 = store.Stats().indexVersion;
    service.DeleteDocument(id);
    const auto v3 = store.Stats().indexVersion;

    EXPECT_GT(v1, v0);
    EXPECT_GT(v2, v1);
    EXPECT_GT(v3, v2);
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsExpectedBm25ScoresForSingleTerm /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto id0 = service.IngestChunk("Cat cat dog");
    const auto id1 = service.IngestChunk("dog bird");
    (void)service.IngestChunk("... ,,, ---");
    const auto id3 = service.IngestChunk("bird CAT");

    const auto result = service.Query("cat", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(scoreFor(*result, id0), 0.79364064, 1e-6);
    EXPECT_DOUBLE_EQ(scoreFor(*result, id1), 0.0);
    EXPECT_NEAR(scoreFor(*result, id3), 0.65487525, 1e-6);
}

TEST(Bm25ServiceTests /*unused*/, QueryRanksDocumentsByCombinedTermMatches /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto id0 = service.IngestChunk("Cat cat dog");
    const auto id1 = service.IngestChunk("dog bird");
    (void)service.IngestChunk("... ,,, ---");
    const auto id3 = service.IngestChunk("bird CAT");

    const auto result = service.Query("cat bird", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(scoreFor(*result, id3), scoreFor(*result, id0));
    EXPECT_GT(scoreFor(*result, id0), scoreFor(*result, id1));
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsEmptyForUnknownTerms /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("Cat cat dog");
    (void)service.IngestChunk("dog bird");

    const auto result = service.Query("wolf", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsEmptyForDelimiterOnlyQuery /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("Cat cat dog");
    (void)service.IngestChunk("dog bird");

    const auto result = service.Query("   ... ,,, ---   ", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsEmptyForEmptyCorpus /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto result = service.Query("cat", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(Bm25ServiceTests /*unused*/, QueryTopKLimitsAndPreservesRanking /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("cat here");
    (void)service.IngestChunk("cat there");
    (void)service.IngestChunk("dog only");

    const auto result = service.QueryTopK("cat", 2);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);
    // Both "cat here" and "cat there" should appear; dog should not.
    for (const auto &r : *result)
    {
        EXPECT_GT(r.score, 0.0);
    }
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsErrorForInvalidParams /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("cat cat");
    (void)service.IngestChunk("dog");

    const auto invalidK = service.Query("cat", bm25::Bm25Params{0.0, 0.75});
    const auto invalidBLow = service.Query("cat", bm25::Bm25Params{1.2, -0.1});
    const auto invalidBHigh = service.Query("cat", bm25::Bm25Params{1.2, 1.1});

    EXPECT_FALSE(invalidK.has_value());
    EXPECT_EQ(invalidK.error(), bm25::Bm25Error::InvalidParams);
    EXPECT_FALSE(invalidBLow.has_value());
    EXPECT_FALSE(invalidBHigh.has_value());
}
