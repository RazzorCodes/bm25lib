#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "bm25.hpp"
#include "store/sqlite.hpp"

namespace
{
struct TempDb
{
    std::string path;
    explicit TempDb(const std::string &name)
        : path((std::filesystem::temp_directory_path() / name).string())
    {
    }
    ~TempDb() { (void)std::filesystem::remove(path); }
    TempDb(const TempDb &) = delete;
    TempDb &operator=(const TempDb &) = delete;
};
} // namespace

TEST(SqliteStoreTests, UpsertTracksStatsAndStableIds)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    const auto first = service.IngestChunk("doc:a", "cat cat dog");
    const auto second = service.IngestChunk("doc:a", "bird bird");

    EXPECT_EQ(first.outcome, bm25::WriteOutcome::Inserted);
    EXPECT_EQ(second.outcome, bm25::WriteOutcome::Updated);
    EXPECT_EQ(first.docId, second.docId);

    const auto stats = service.Stats();
    EXPECT_EQ(stats.documentCount, 1U);
    EXPECT_EQ(stats.totalTokens, 2U);
    EXPECT_DOUBLE_EQ(stats.avgDocumentLength, 2.0);
    EXPECT_EQ(stats.indexVersion, 2U);
}

TEST(SqliteStoreTests, DocumentFrequenciesReflectReplacedContent)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat cat dog");
    (void)service.IngestChunk("d1", "dog bird");
    (void)service.IngestChunk("d0", "bird");

    const auto freqs = store.DocumentFrequencies();
    EXPECT_EQ(freqs.count("cat"), 0U);
    ASSERT_EQ(freqs.count("dog"), 1U);
    ASSERT_EQ(freqs.count("bird"), 1U);
    EXPECT_EQ(freqs.at("dog"), 1U);
    EXPECT_EQ(freqs.at("bird"), 2U);
}

TEST(SqliteStoreTests, FetchPostingsReturnsOnlyQueriedTerms)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    const auto catId = service.IngestChunk("d0", "cat dog").docId;
    (void)service.IngestChunk("d1", "bird bird");

    const auto postings = store.FetchPostings({"cat"});
    ASSERT_EQ(postings.size(), 1U);
    EXPECT_EQ(postings[0].first, catId);
    EXPECT_EQ(postings[0].second.tokenCount, 2U);
    EXPECT_EQ(postings[0].second.termFrequencies.size(), 1U);
    EXPECT_EQ(postings[0].second.termFrequencies.at("cat"), 1U);
    EXPECT_EQ(postings[0].second.termFrequencies.count("dog"), 0U);
}

TEST(SqliteStoreTests, DeleteAndClearKeepStateConsistent)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    const auto id = service.IngestChunk("d0", "cat dog").docId;
    (void)service.IngestChunk("d1", "bird");

    EXPECT_TRUE(service.DeleteDocument(id));
    EXPECT_FALSE(service.DeleteDocument(id));

    const auto afterDelete = service.Stats();
    EXPECT_EQ(afterDelete.documentCount, 1U);
    EXPECT_EQ(afterDelete.totalTokens, 1U);
    EXPECT_EQ(afterDelete.indexVersion, 3U);

    store.Clear();

    const auto afterClear = service.Stats();
    EXPECT_EQ(afterClear.documentCount, 0U);
    EXPECT_EQ(afterClear.totalTokens, 0U);
    EXPECT_EQ(afterClear.indexVersion, 4U);
}

TEST(SqliteStoreTests, EmptyCorpusQueryReturnsEmpty)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    const auto result = service.Query("cat");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(SqliteStoreTests, UnknownTermReturnsEmpty)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat dog");
    const auto result = service.Query("wolf");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(SqliteStoreTests, DelimiterOnlyQueryReturnsEmpty)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat dog");
    const auto result = service.Query("... ,,, ---");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(SqliteStoreTests, TopKBehaviorViaSqliteAdapter)
{
    Store::Sqlite store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat here");
    (void)service.IngestChunk("d1", "cat there");
    (void)service.IngestChunk("d2", "dog only");

    const auto result = service.QueryTopK("cat", 2);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);
    for (const auto &r : *result)
    {
        EXPECT_GT(r.score, 0.0);
    }
}

TEST(SqliteStoreTests, RestartPersistencePreservesCorpusStats)
{
    TempDb tmp("bm25_restart_stats.db");

    bm25::CorpusStats before;
    {
        Store::Sqlite store(tmp.path);
        bm25::Bm25 service(store);
        (void)service.IngestChunk("d0", "cat cat dog");
        (void)service.IngestChunk("d1", "bird");
        before = service.Stats();
    }

    Store::Sqlite store2(tmp.path);
    bm25::Bm25 service2(store2);
    const auto after = service2.Stats();

    EXPECT_EQ(after.documentCount, before.documentCount);
    EXPECT_EQ(after.totalTokens, before.totalTokens);
    EXPECT_DOUBLE_EQ(after.avgDocumentLength, before.avgDocumentLength);
    EXPECT_EQ(after.indexVersion, before.indexVersion);
}

TEST(SqliteStoreTests, RestartPersistencePreservesQueryResults)
{
    TempDb tmp("bm25_restart_query.db");

    bm25::RankedResults before;
    {
        Store::Sqlite store(tmp.path);
        bm25::Bm25 service(store);
        (void)service.IngestChunk("d0", "cat cat dog");
        (void)service.IngestChunk("d1", "cat bird");
        (void)service.IngestChunk("d2", "dog only");
        auto r = service.Query("cat");
        ASSERT_TRUE(r.has_value());
        before = std::move(*r);
    }

    Store::Sqlite store2(tmp.path);
    bm25::Bm25 service2(store2);
    const auto after = service2.Query("cat");
    ASSERT_TRUE(after.has_value());

    ASSERT_EQ(after->size(), before.size());
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        EXPECT_EQ((*after)[i].docId, before[i].docId);
        EXPECT_NEAR((*after)[i].score, before[i].score, 1e-9);
    }
}

TEST(SqliteStoreTests, DeterministicTieOrderingAfterRestart)
{
    TempDb tmp("bm25_restart_tie.db");

    bm25::RankedResults before;
    {
        Store::Sqlite store(tmp.path);
        bm25::Bm25 service(store);
        // Equal-length docs with same term → equal BM25 scores; tie broken by docId asc.
        (void)service.IngestChunk("d0", "cat fox");
        (void)service.IngestChunk("d1", "cat dog");
        auto r = service.Query("cat");
        ASSERT_TRUE(r.has_value());
        ASSERT_EQ(r->size(), 2U);
        before = std::move(*r);
    }

    Store::Sqlite store2(tmp.path);
    bm25::Bm25 service2(store2);
    const auto after = service2.Query("cat");
    ASSERT_TRUE(after.has_value());
    ASSERT_EQ(after->size(), 2U);

    EXPECT_EQ((*after)[0].docId, before[0].docId);
    EXPECT_EQ((*after)[1].docId, before[1].docId);
    EXPECT_NEAR((*after)[0].score, (*after)[1].score, 1e-9);
    EXPECT_LT((*after)[0].docId, (*after)[1].docId);
}
