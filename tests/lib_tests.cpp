#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

#include "lib.hpp"

TEST(LibTests, CountsTotalTokensAndPerTermFrequency)
{
    const auto [size, tf] = ingest_text("The willy quick fox doesn't jump over the lazy brown dog.");

    EXPECT_EQ(size, 11U);
    EXPECT_EQ(tf.size(), 10U);
    EXPECT_EQ(tf.at("the"), 2U);
    EXPECT_EQ(tf.at("doesn't"), 1U);
    EXPECT_EQ(tf.at("dog"), 1U);
}

TEST(LibTests, MergesRepeatedTokensAfterNormalization)
{
    const auto [size, tf] = ingest_text("Go go GO!");

    EXPECT_EQ(size, 3U);
    ASSERT_EQ(tf.size(), 1U);
    EXPECT_EQ(tf.at("go"), 3U);
}

TEST(LibTests, ReturnsEmptyFrequencyMapForDelimiterOnlyInput)
{
    const auto [size, tf] = ingest_text("... ,,, ---");

    EXPECT_EQ(size, 0U);
    EXPECT_TRUE(tf.empty());
}

TEST(LibTests, IngestMultipleTextsBuildsDocumentFrequenciesAndPerDocumentTfs)
{
    const std::vector<std::string_view> texts{
        "Cat cat dog",
        "dog bird",
        "... ,,, ---",
        "bird CAT"
    };

    const auto [avgdl, df, docs] = ingest_multiple_texts(texts);

    EXPECT_DOUBLE_EQ(avgdl, 1.75);
    ASSERT_EQ(docs.size(), texts.size());
    ASSERT_EQ(df.size(), 3U);
    EXPECT_EQ(df.at("cat"), 2U);
    EXPECT_EQ(df.at("dog"), 2U);
    EXPECT_EQ(df.at("bird"), 2U);

    EXPECT_EQ(std::get<0>(docs[0]), 3U);
    EXPECT_EQ(std::get<1>(docs[0]).at("cat"), 2U);
    EXPECT_EQ(std::get<1>(docs[0]).at("dog"), 1U);

    EXPECT_EQ(std::get<0>(docs[1]), 2U);
    EXPECT_EQ(std::get<1>(docs[1]).at("dog"), 1U);
    EXPECT_EQ(std::get<1>(docs[1]).at("bird"), 1U);

    EXPECT_EQ(std::get<0>(docs[2]), 0U);
    EXPECT_TRUE(std::get<1>(docs[2]).empty());

    EXPECT_EQ(std::get<0>(docs[3]), 2U);
    EXPECT_EQ(std::get<1>(docs[3]).at("bird"), 1U);
    EXPECT_EQ(std::get<1>(docs[3]).at("cat"), 1U);
}

TEST(LibTests, IngestMultipleTextsHandlesEmptyCorpus)
{
    const std::vector<std::string_view> texts{};

    const auto [avgdl, df, docs] = ingest_multiple_texts(texts);

    EXPECT_DOUBLE_EQ(avgdl, 0.0);
    EXPECT_TRUE(df.empty());
    EXPECT_TRUE(docs.empty());
}

TEST(LibTests, QueryReturnsExpectedBm25ScoresForSingleTerm)
{
    const std::vector<std::string_view> texts{
        "Cat cat dog",
        "dog bird",
        "... ,,, ---",
        "bird CAT"
    };

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("cat", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_NEAR(scores[0], 0.79364064, 1e-6);
    EXPECT_DOUBLE_EQ(scores[1], 0.0);
    EXPECT_DOUBLE_EQ(scores[2], 0.0);
    EXPECT_NEAR(scores[3], 0.65487525, 1e-6);
}

TEST(LibTests, QueryRanksDocumentsByCombinedTermMatches)
{
    const std::vector<std::string_view> texts{
        "Cat cat dog",
        "dog bird",
        "... ,,, ---",
        "bird CAT"
    };

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("cat bird", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_GT(scores[3], scores[0]);
    EXPECT_GT(scores[0], scores[1]);
    EXPECT_GT(scores[1], scores[2]);
}

TEST(LibTests, QueryReturnsZeroScoresForUnknownTerms)
{
    const std::vector<std::string_view> texts{
        "Cat cat dog",
        "dog bird"
    };

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("wolf", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_TRUE(std::all_of(scores.begin(), scores.end(), [](const double score) {
        return score == 0.0;
    }));
}

TEST(LibTests, QueryReturnsZeroScoresForEmptyQuery)
{
    const std::vector<std::string_view> texts{
        "Cat cat dog",
        "dog bird"
    };

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("   ... ,,, ---   ", corpus, 1.2, 0.75);

    ASSERT_EQ(scores.size(), texts.size());
    EXPECT_TRUE(std::all_of(scores.begin(), scores.end(), [](const double score) {
        return score == 0.0;
    }));
}

TEST(LibTests, QueryReturnsEmptyScoresForEmptyCorpus)
{
    const std::vector<std::string_view> texts{};

    const auto corpus = ingest_multiple_texts(texts);
    const auto scores = query("cat", corpus, 1.2, 0.75);

    EXPECT_TRUE(scores.empty());
}
