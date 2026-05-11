#include <gtest/gtest.h>

#include <vector>

#include "core/ranker.hpp"
#include "core/scorer.hpp"
#include "core/types.hpp"

TEST(CoreTests /*unused*/, RankScoresAppliesTopKAndStableTieBreakByDocId /*unused*/)
{
    const Core::RankedResults scored{{0, 0.3}, {1, 0.5}, {2, 0.5}, {3, 0.1}};
    const auto ranked = Core::RankScores(scored, 2);

    ASSERT_EQ(ranked.size(), 2U);
    EXPECT_EQ(ranked[0].docId, 1U);
    EXPECT_EQ(ranked[1].docId, 2U);
    EXPECT_DOUBLE_EQ(ranked[0].score, 0.5);
    EXPECT_DOUBLE_EQ(ranked[1].score, 0.5);
}

TEST(CoreTests /*unused*/, ScoreQueryReturnsErrorWhenParamsAreInvalid /*unused*/)
{
    const Core::TermMap queryTerms{{"cat", 1}};
    const std::vector<std::pair<Core::DocumentId, Core::IngestResult>> postings{
        {42U, Core::IngestResult{2U, {{"cat", 2U}}}}};
    const Core::CorpusStats stats{1U, 2U, 2.0, 0U};
    const Core::DocFrequencyMap docFreqs{{"cat", 1U}};

    const auto result = Core::ScoreQuery(queryTerms, postings, stats, docFreqs,
                                         Core::Bm25Params{0.0, 0.75});

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), Core::Bm25Error::InvalidParams);
}

TEST(CoreTests /*unused*/, ScoreQueryReturnsEmptyForEmptyQueryTerms /*unused*/)
{
    const Core::TermMap queryTerms{};
    const std::vector<std::pair<Core::DocumentId, Core::IngestResult>> postings{
        {42U, Core::IngestResult{2U, {{"cat", 2U}}}}};
    const Core::CorpusStats stats{1U, 2U, 2.0, 0U};
    const Core::DocFrequencyMap docFreqs{{"cat", 1U}};

    const auto result = Core::ScoreQuery(queryTerms, postings, stats, docFreqs);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}
