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

    (void)service.IngestChunk("k1", "The willy quick fox doesn't jump over the lazy brown dog.");

    const auto stats = service.Stats();
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

    (void)service.IngestChunk("k1", "Go go GO!");

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

    (void)service.IngestChunk("k1", "... ,,, ---");

    const auto stats = service.Stats();
    ASSERT_EQ(stats.documentCount, 1U);

    // Empty-token docs are in the store but match no term query.
    const auto postings = store.FetchPostings({"anything"});
    EXPECT_TRUE(postings.empty());
}

// --- Ingest/lifecycle contract tests ---

TEST(Bm25ServiceTests /*unused*/, IngestChunkReportsInsertedForNewKey /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto result = service.IngestChunk("doc:a", "hello world");
    EXPECT_EQ(result.outcome, bm25::WriteOutcome::Inserted);
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkReportsUpdatedForExistingKey /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("doc:a", "hello world");
    const auto result = service.IngestChunk("doc:a", "hello world");

    EXPECT_EQ(result.outcome, bm25::WriteOutcome::Updated);
}

TEST(Bm25ServiceTests /*unused*/, SameKeyReturnsStableDocumentIdOnRepeatedIngest /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto r1 = service.IngestChunk("doc:hello", "hello world");
    const auto r2 = service.IngestChunk("doc:hello", "hello world");

    EXPECT_EQ(r1.docId, r2.docId);
    EXPECT_EQ(service.Stats().documentCount, 1U);
    EXPECT_EQ(service.Stats().totalTokens, 2U);
}

TEST(Bm25ServiceTests /*unused*/, DifferentKeysAreIndependentDocumentsEvenWithIdenticalContent /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto r1 = service.IngestChunk("key:a", "hello world");
    const auto r2 = service.IngestChunk("key:b", "hello world");

    EXPECT_NE(r1.docId, r2.docId);
    EXPECT_EQ(r1.outcome, bm25::WriteOutcome::Inserted);
    EXPECT_EQ(r2.outcome, bm25::WriteOutcome::Inserted);
    EXPECT_EQ(service.Stats().documentCount, 2U);
    EXPECT_EQ(service.Stats().totalTokens, 4U);
}

TEST(Bm25ServiceTests /*unused*/, UpdateByKeyReplacesContentAndCorrectlyUpdatesCorpusStats /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto r1 = service.IngestChunk("doc:1", "cat cat dog");
    EXPECT_EQ(r1.outcome, bm25::WriteOutcome::Inserted);
    EXPECT_EQ(store.DocumentFrequencies().at("cat"), 1U);

    // Update same key with different content.
    const auto r2 = service.IngestChunk("doc:1", "bird bird bird");
    EXPECT_EQ(r2.docId, r1.docId);  // same id — update, not insert
    EXPECT_EQ(r2.outcome, bm25::WriteOutcome::Updated);
    EXPECT_EQ(service.Stats().documentCount, 1U);
    EXPECT_EQ(service.Stats().totalTokens, 3U);

    const auto freqs = store.DocumentFrequencies();
    EXPECT_EQ(freqs.count("cat"), 0U);  // removed
    EXPECT_EQ(freqs.at("bird"), 1U);    // new content
}

TEST(Bm25ServiceTests /*unused*/, IngestChunkTracksDocumentFrequencies /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat cat dog");
    (void)service.IngestChunk("d1", "dog bird");
    (void)service.IngestChunk("d2", "... ,,, ---");
    (void)service.IngestChunk("d3", "bird CAT");

    const auto freqs = store.DocumentFrequencies();
    ASSERT_EQ(freqs.size(), 3U);
    EXPECT_EQ(freqs.at("cat"), 2U);
    EXPECT_EQ(freqs.at("dog"), 2U);
    EXPECT_EQ(freqs.at("bird"), 2U);

    EXPECT_DOUBLE_EQ(service.Stats().avgDocumentLength, 1.75);
}

TEST(Bm25ServiceTests /*unused*/, DeleteDocumentReturnsTrueAndRemovesIt /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat cat dog");
    const auto idDog = service.IngestChunk("d1", "dog bird").docId;
    (void)service.IngestChunk("d2", "bird CAT");

    ASSERT_EQ(service.Stats().documentCount, 3U);
    EXPECT_TRUE(service.DeleteDocument(idDog));

    EXPECT_EQ(service.Stats().documentCount, 2U);
    const auto freqs = store.DocumentFrequencies();
    EXPECT_EQ(freqs.at("dog"), 1U);  // only in d0 now
    EXPECT_EQ(freqs.at("bird"), 1U); // only in d2 now
}

TEST(Bm25ServiceTests /*unused*/, DeleteDocumentReturnsFalseForUnknownId /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("k1", "cat");
    ASSERT_EQ(service.Stats().documentCount, 1U);

    EXPECT_FALSE(service.DeleteDocument(99999U));
    EXPECT_EQ(service.Stats().documentCount, 1U);
}

TEST(Bm25ServiceTests /*unused*/, IndexVersionIncrementsOnEveryWrite /*unused*/)
{
    Store::InMemory store;
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

// --- Query tests ---

TEST(Bm25ServiceTests /*unused*/, QueryReturnsExpectedBm25ScoresForSingleTerm /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    const auto id0 = service.IngestChunk("d0", "Cat cat dog").docId;
    const auto id1 = service.IngestChunk("d1", "dog bird").docId;
    (void)service.IngestChunk("d2", "... ,,, ---");
    const auto id3 = service.IngestChunk("d3", "bird CAT").docId;

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

    const auto id0 = service.IngestChunk("d0", "Cat cat dog").docId;
    const auto id1 = service.IngestChunk("d1", "dog bird").docId;
    (void)service.IngestChunk("d2", "... ,,, ---");
    const auto id3 = service.IngestChunk("d3", "bird CAT").docId;

    const auto result = service.Query("cat bird", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_GT(scoreFor(*result, id3), scoreFor(*result, id0));
    EXPECT_GT(scoreFor(*result, id0), scoreFor(*result, id1));
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsEmptyForUnknownTerms /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "Cat cat dog");
    (void)service.IngestChunk("d1", "dog bird");

    const auto result = service.Query("wolf", bm25::Bm25Params{1.2, 0.75});
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsEmptyForDelimiterOnlyQuery /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "Cat cat dog");
    (void)service.IngestChunk("d1", "dog bird");

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

    (void)service.IngestChunk("d0", "cat here");
    (void)service.IngestChunk("d1", "cat there");
    (void)service.IngestChunk("d2", "dog only");

    const auto result = service.QueryTopK("cat", 2);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 2U);
    // Both "cat here" and "cat there" should appear; "dog only" should not.
    for (const auto &r : *result)
    {
        EXPECT_GT(r.score, 0.0);
    }
}

TEST(Bm25ServiceTests /*unused*/, QueryReturnsErrorForInvalidParams /*unused*/)
{
    Store::InMemory store;
    bm25::Bm25 service(store);

    (void)service.IngestChunk("d0", "cat cat");
    (void)service.IngestChunk("d1", "dog");

    const auto invalidK = service.Query("cat", bm25::Bm25Params{0.0, 0.75});
    const auto invalidBLow = service.Query("cat", bm25::Bm25Params{1.2, -0.1});
    const auto invalidBHigh = service.Query("cat", bm25::Bm25Params{1.2, 1.1});

    EXPECT_FALSE(invalidK.has_value());
    EXPECT_EQ(invalidK.error(), bm25::Bm25Error::InvalidParams);
    EXPECT_FALSE(invalidBLow.has_value());
    EXPECT_FALSE(invalidBHigh.has_value());
}
