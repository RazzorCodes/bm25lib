#pragma once

#include <cstddef>
#include <cmath>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "utils/string_utils.hpp"

using term_frequency_map = std::unordered_map<std::string, std::size_t>;
using doc_frequency_map = std::unordered_map<std::string, std::size_t>;
using ingest_result = std::tuple<std::size_t, term_frequency_map>;
using complete_result = std::tuple<double, doc_frequency_map, std::vector<ingest_result>>;
using score_vector = std::vector<double>;

inline ingest_result ingest_text(std::string_view text)
{
    std::size_t size = 0;
    term_frequency_map tf;

    tokenize(text, [&size, &tf](std::string token) {
        ++size;
        ++tf[std::move(token)];
    });

    return {size, std::move(tf)};
}

inline void add_to_df_from_tf(const term_frequency_map& tf, doc_frequency_map& df)
{
    for (const auto& entry : tf)
    {
        ++df[entry.first];
    }
}

inline complete_result ingest_multiple_texts(const std::vector<std::string_view>& texts)
{
    std::size_t dsum = 0;
    doc_frequency_map df;
    std::vector<ingest_result> tfs;
    tfs.reserve(texts.size());

    for (const auto text : texts)
    {
        tfs.push_back(ingest_text(text));
        dsum += std::get<0>(tfs.back());
        add_to_df_from_tf(std::get<1>(tfs.back()), df);
    }

    auto avgdl = texts.size() == 0 ? 0.0f : (double)dsum / texts.size();

    return {avgdl, df, tfs};
}

inline score_vector query(const std::string_view q, const complete_result& res, const double k, const double b)
{
    term_frequency_map qt;
    tokenize(q, [&qt](std::string token) {
        ++qt[std::move(token)];
    });

    const auto avgdl = std::get<0>(res);
    const auto& vocab = std::get<1>(res);
    const auto& doc_res = std::get<2>(res);
    score_vector scores(doc_res.size(), 0.0);

    if (qt.empty() || doc_res.empty())
    {
        return scores;
    }

    for (const auto& [term, query_tf] : qt)
    {
        const auto it = vocab.find(term);
        if (it == vocab.end())
        {
            continue;
        }

        const auto doc_count = static_cast<double>(doc_res.size());
        const auto doc_frequency = static_cast<double>(it->second);
        const auto idf = std::log(1.0 + (doc_count - doc_frequency + 0.5) / (doc_frequency + 0.5));

        for (std::size_t doc_idx = 0; doc_idx < doc_res.size(); ++doc_idx)
        {
            const auto& [dl, doc_tf] = doc_res[doc_idx];
            const auto tf_it = doc_tf.find(term);
            if (tf_it == doc_tf.end())
            {
                continue;
            }

            const auto tf_td = static_cast<double>(tf_it->second);
            const auto length_factor = avgdl == 0.0 ? 1.0 : (1.0 - b + b * static_cast<double>(dl) / avgdl);
            const auto den = tf_td + k * length_factor;
            if (den == 0.0)
            {
                continue;
            }

            const auto tf_norm = tf_td * (k + 1.0) / den;
            scores[doc_idx] += static_cast<double>(query_tf) * idf * tf_norm;
        }
    }

    return scores;
}
