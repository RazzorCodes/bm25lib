#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

static constexpr auto characters()
{
    std::array<bool, 256> chrs{};

    for (auto c = 'a'; c <= 'z'; c++)
    {
        chrs.at(static_cast<unsigned char>(c)) = true;
    }
    for (auto c = '0'; c <= '9'; c++)
    {
        chrs.at(static_cast<unsigned char>(c)) = true;
    }

    return chrs;
}

static constexpr auto kCharacters = characters();

static constexpr bool is_delimiter(const unsigned char chr)
{
    if (chr == '\'')
    {
        return false;
    }

    if (kCharacters.at(chr))
    {
        return false;
    }

    return true;
}

static constexpr auto lower()
{
    std::array<char, 256> chrs{};

    for (auto c = 0; c <= 255; c++)
    {
        chrs.at(static_cast<unsigned char>(c)) = static_cast<char>(c);
    }
    for (auto c = 'A'; c <= 'Z'; c++)
    {
        chrs.at(static_cast<unsigned char>(c)) = static_cast<char>(c + ('a' - 'A'));
    }

    return chrs;
}

static constexpr auto kLower = lower();

inline void tokenize(std::string_view text, const std::function<void(std::string)> &emit)
{
    std::string tok;
    tok.reserve(32);

    for (const char c : text)
    {
        const auto normalized = kLower.at(static_cast<unsigned char>(c));

        if (is_delimiter(static_cast<unsigned char>(normalized)))
        {
            if (!tok.empty())
            {
                emit(tok);
                tok.clear();
            }

            continue;
        }

        tok.push_back(normalized);
    }

    if (!tok.empty())
    {
        emit(tok);
    }
}
