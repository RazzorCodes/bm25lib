#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "utils/string_utils.hpp"

namespace
{

std::vector<std::string> collect_tokens(std::string_view text)
{
    std::vector<std::string> tokens;

    tokenize(text, [&](std::string token) { tokens.push_back(std::move(token)); });

    return tokens;
}

} // namespace

TEST(TokenizeTest /*unused*/, NormalizesCaseAndSplitsWords /*unused*/)
{
    EXPECT_EQ(collect_tokens("The Quick Brown FOX"),
              (std::vector<std::string>{"the", "quick", "brown", "fox"}));
}

TEST(TokenizeTest /*unused*/, IgnoresRepeatedLeadingAndTrailingDelimiters /*unused*/)
{
    EXPECT_EQ(collect_tokens("  Hello,,   world!!!  "),
              (std::vector<std::string>{"hello", "world"}));
}

TEST(TokenizeTest /*unused*/, KeepsApostrophesInsideTokens /*unused*/)
{
    EXPECT_EQ(collect_tokens("Don't stop believin'"),
              (std::vector<std::string>{"don't", "stop", "believin'"}));
}

TEST(TokenizeTest /*unused*/, KeepsDigitsAndSplitsOnNonWordPunctuation /*unused*/)
{
    EXPECT_EQ(collect_tokens("R2D2 version 2.0"),
              (std::vector<std::string>{"r2d2", "version", "2", "0"}));
}

TEST(TokenizeTest /*unused*/, ReturnsNoTokensForDelimiterOnlyInput /*unused*/)
{
    EXPECT_TRUE(collect_tokens("... ,,, ---").empty());
}
