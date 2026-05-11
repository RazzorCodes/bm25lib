#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "lib.hpp"

TEST(LibTests /*unused*/, CountsTotalTokensAndPerTermFrequency /*unused*/)
{
    const auto ingested = ingest_text("The willy quick fox doesn't jump over the lazy brown dog.");

    EXPECT_EQ(ingested.tokenCount, 11U);
    EXPECT_EQ(ingested.termFrequencies.size(), 10U);
    EXPECT_EQ(ingested.termFrequencies.at("the"), 2U);
    EXPECT_EQ(ingested.termFrequencies.at("doesn't"), 1U);
    EXPECT_EQ(ingested.termFrequencies.at("dog"), 1U);
}

TEST(LibTests /*unused*/, MergesRepeatedTokensAfterNormalization /*unused*/)
{
    const auto ingested = ingest_text("Go go GO!");

    EXPECT_EQ(ingested.tokenCount, 3U);
    ASSERT_EQ(ingested.termFrequencies.size(), 1U);
    EXPECT_EQ(ingested.termFrequencies.at("go"), 3U);
}

TEST(LibTests /*unused*/, ReturnsEmptyFrequencyMapForDelimiterOnlyInput /*unused*/)
{
    const auto ingested = ingest_text("... ,,, ---");

    EXPECT_EQ(ingested.tokenCount, 0U);
    EXPECT_TRUE(ingested.termFrequencies.empty());
}

TEST(LibTests /*unused*/, IngestMultipleTextsBuildsDocumentFrequenciesAndPerDocumentTfs /*unused*/)
{
    const std::vector<std::string_view> texts{"Cat cat dog", "dog bird", "... ,,, ---", "bird CAT"};

    const auto corpus = ingest_multiple_texts(texts);

    EXPECT_DOUBLE_EQ(corpus.avgDocumentLength, 1.75);
    ASSERT_EQ(corpus.documents.size(), texts.size());
    ASSERT_EQ(corpus.docFrequencies.size(), 3U);
    EXPECT_EQ(corpus.docFrequencies.at("cat"), 2U);
    EXPECT_EQ(corpus.docFrequencies.at("dog"), 2U);
    EXPECT_EQ(corpus.docFrequencies.at("bird"), 2U);

    EXPECT_EQ(corpus.documents[0].tokenCount, 3U);
    EXPECT_EQ(corpus.documents[0].termFrequencies.at("cat"), 2U);
    EXPECT_EQ(corpus.documents[0].termFrequencies.at("dog"), 1U);

    EXPECT_EQ(corpus.documents[1].tokenCount, 2U);
    EXPECT_EQ(corpus.documents[1].termFrequencies.at("dog"), 1U);
    EXPECT_EQ(corpus.documents[1].termFrequencies.at("bird"), 1U);

    EXPECT_EQ(corpus.documents[2].tokenCount, 0U);
    EXPECT_TRUE(corpus.documents[2].termFrequencies.empty());

    EXPECT_EQ(corpus.documents[3].tokenCount, 2U);
    EXPECT_EQ(corpus.documents[3].termFrequencies.at("bird"), 1U);
    EXPECT_EQ(corpus.documents[3].termFrequencies.at("cat"), 1U);
}

TEST(LibTests /*unused*/, IngestMultipleTextsHandlesEmptyCorpus /*unused*/)
{
    const std::vector<std::string_view> texts{};

    const auto corpus = ingest_multiple_texts(texts);

    EXPECT_DOUBLE_EQ(corpus.avgDocumentLength, 0.0);
    EXPECT_TRUE(corpus.docFrequencies.empty());
    EXPECT_TRUE(corpus.documents.empty());
}

TEST(LibTests /*unused*/, QueryReturnsExpectedBm25ScoresForSingleTerm /*unused*/)
{
    const std::vector<std::string_view> texts{"Cat cat dog", "dog bird", "... ,,, ---", "bird CAT"};

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("cat", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_NEAR(scores[0], 0.79364064, 1e-6);
    EXPECT_DOUBLE_EQ(scores[1], 0.0);
    EXPECT_DOUBLE_EQ(scores[2], 0.0);
    EXPECT_NEAR(scores[3], 0.65487525, 1e-6);
}

TEST(LibTests /*unused*/, QueryRanksDocumentsByCombinedTermMatches /*unused*/)
{
    const std::vector<std::string_view> texts{"Cat cat dog", "dog bird", "... ,,, ---", "bird CAT"};

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("cat bird", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_GT(scores[3], scores[0]);
    EXPECT_GT(scores[0], scores[1]);
    EXPECT_GT(scores[1], scores[2]);
}

TEST(LibTests /*unused*/, QueryReturnsZeroScoresForUnknownTerms /*unused*/)
{
    const std::vector<std::string_view> texts{"Cat cat dog", "dog bird"};

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("wolf", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_TRUE(
        std::all_of(scores.begin(), scores.end(), [](const double score) { return score == 0.0; }));
}

TEST(LibTests /*unused*/, QueryReturnsZeroScoresForEmptyQuery /*unused*/)
{
    const std::vector<std::string_view> texts{"Cat cat dog", "dog bird"};

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("   ... ,,, ---   ", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_TRUE(
        std::all_of(scores.begin(), scores.end(), [](const double score) { return score == 0.0; }));
}

TEST(LibTests /*unused*/, QueryReturnsEmptyScoresForEmptyCorpus /*unused*/)
{
    const std::vector<std::string_view> texts{};

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("cat", corpus, 1.2, 0.75);

    EXPECT_TRUE(scores.empty());
}

TEST(LibTests /*unused*/, QueryTopKReturnsRankedResultsWithDeterministicTieOrdering /*unused*/)
{
    const std::vector<std::string_view> texts{"cat", "cat", "dog"};

    const auto corpus = ingest_multiple_texts(texts);
    const auto ranked = query_top_k("cat", corpus, 2);

    ASSERT_EQ(ranked.size(), 2U);
    EXPECT_EQ(ranked[0].docId, 0U);
    EXPECT_EQ(ranked[1].docId, 1U);
    EXPECT_DOUBLE_EQ(ranked[0].score, ranked[1].score);
}

TEST(LibTests /*unused*/, QueryReturnsZeroScoresWhenConfigIsInvalid /*unused*/)
{
    const std::vector<std::string_view> texts{"cat cat", "dog"};
    const auto corpus = ingest_multiple_texts(texts);

    const auto invalid_k = query_top_k("cat", corpus, 2, Bm25Config{0.0, 0.75});
    const auto invalid_b_low = query_top_k("cat", corpus, 2, Bm25Config{1.2, -0.1});
    const auto invalid_b_high = query_top_k("cat", corpus, 2, Bm25Config{1.2, 1.1});

    ASSERT_EQ(invalid_k.size(), 2U);
    ASSERT_EQ(invalid_b_low.size(), 2U);
    ASSERT_EQ(invalid_b_high.size(), 2U);

    EXPECT_DOUBLE_EQ(invalid_k[0].score, 0.0);
    EXPECT_DOUBLE_EQ(invalid_k[1].score, 0.0);
    EXPECT_DOUBLE_EQ(invalid_b_low[0].score, 0.0);
    EXPECT_DOUBLE_EQ(invalid_b_low[1].score, 0.0);
    EXPECT_DOUBLE_EQ(invalid_b_high[0].score, 0.0);
    EXPECT_DOUBLE_EQ(invalid_b_high[1].score, 0.0);
}
