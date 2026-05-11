#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ingestion.hpp"
#include "core/types.hpp"
#include "store/in_memory.hpp"
#include "utils/string_utils.hpp"

using term_frequency_map = Core::TermMap;
using doc_frequency_map = Store::DocFrequencyMap;
using score_vector = std::vector<double>;

struct IngestResult
{
    std::size_t tokenCount = 0;
    term_frequency_map termFrequencies;
};

struct CompleteResult
{
    double avgDocumentLength = 0.0;
    doc_frequency_map docFrequencies;
    std::vector<IngestResult> documents;
};

struct RankedResult
{
    Core::DocumentId docId = 0;
    double score = 0.0;
};

using RankedResults = std::vector<RankedResult>;

struct Bm25Config
{
    double k = 1.2;
    double b = 0.75;

    static Bm25Config defaults()
    {
        return {};
    }

    [[nodiscard]] bool valid() const
    {
        return k > 0.0 && b >= 0.0 && b <= 1.0;
    }
};

class Analyzer
{
public:
    [[nodiscard]] static IngestResult ingest_text(const std::string_view text)
    {
        const auto res = Core::Ingest(text);
        return {res.tokenCount, res.termFrequencies};
    }
};

class Ingester
{
public:
    [[nodiscard]] static CompleteResult
    ingest_multiple_texts(const std::vector<std::string_view> &texts)
    {
        Store::InMemory store;

        for (const auto text : texts)
        {
            store.UpsertDocument(Core::Ingest(text));
        }

        CompleteResult result;
        const auto stats = store.Stats();
        result.avgDocumentLength = stats.avgDocumentLength;
        result.docFrequencies = store.DocumentFrequencies();

        const auto &docs = store.Documents();
        result.documents.reserve(docs.size());
        for (const auto &doc : docs)
        {
            result.documents.push_back({doc.tokenCount, doc.termFrequencies});
        }

        return result;
    }
};

class Scorer
{
public:
    [[nodiscard]] static score_vector query_scores(const std::string_view q,
                                                   const CompleteResult &res,
                                                   const Bm25Config config = Bm25Config::defaults())
    {
        if (!config.valid())
        {
            return score_vector(res.documents.size(), 0.0);
        }

        term_frequency_map query_terms;
        tokenize(q, [&query_terms](const std::string &token) { ++query_terms[token]; });

        score_vector scores(res.documents.size(), 0.0);
        if (query_terms.empty() || res.documents.empty())
        {
            return scores;
        }

        for (const auto &[term, query_tf] : query_terms)
        {
            const auto it = res.docFrequencies.find(term);
            if (it == res.docFrequencies.end())
            {
                continue;
            }

            const auto doc_count = static_cast<double>(res.documents.size());
            const auto doc_frequency = static_cast<double>(it->second);
            const auto idf =
                std::log(1.0 + ((doc_count - doc_frequency + 0.5) / (doc_frequency + 0.5)));

            for (std::size_t doc_idx = 0; doc_idx < res.documents.size(); ++doc_idx)
            {
                const auto &doc = res.documents[doc_idx];
                const auto tf_it = doc.termFrequencies.find(term);
                if (tf_it == doc.termFrequencies.end())
                {
                    continue;
                }

                const auto tf_td = static_cast<double>(tf_it->second);
                const auto length_factor = res.avgDocumentLength == 0.0
                                               ? 1.0
                                               : (1.0 - config.b +
                                                  (config.b * static_cast<double>(doc.tokenCount) /
                                                   res.avgDocumentLength));
                const auto den = tf_td + (config.k * length_factor);
                if (den == 0.0)
                {
                    continue;
                }

                const auto tf_norm = tf_td * (config.k + 1.0) / den;
                scores[doc_idx] += static_cast<double>(query_tf) * idf * tf_norm;
            }
        }

        return scores;
    }
};

class ResultRanker
{
public:
    [[nodiscard]] static RankedResults rank(const score_vector &scores, std::size_t top_k = 0)
    {
        RankedResults ranked;
        ranked.reserve(scores.size());
        for (std::size_t docId = 0; docId < scores.size(); ++docId)
        {
            ranked.push_back({docId, scores[docId]});
        }

        std::sort(ranked.begin(), ranked.end(),
                  [](const RankedResult &lhs, const RankedResult &rhs) {
                      if (lhs.score != rhs.score)
                      {
                          return lhs.score > rhs.score;
                      }

                      return lhs.docId < rhs.docId;
                  });

        if (top_k > 0 && top_k < ranked.size())
        {
            ranked.resize(top_k);
        }

        return ranked;
    }
};

class Bm25Service
{
public:
    explicit Bm25Service(Bm25Config config = Bm25Config::defaults()) : config(config) {}

    [[nodiscard]] static IngestResult ingest_text(const std::string_view text)
    {
        return Analyzer::ingest_text(text);
    }

    [[nodiscard]] static CompleteResult
    ingest_multiple_texts(const std::vector<std::string_view> &texts)
    {
        return Ingester::ingest_multiple_texts(texts);
    }

    [[nodiscard]] score_vector query_scores(const std::string_view q,
                                            const CompleteResult &corpus) const
    {
        return Scorer::query_scores(q, corpus, config);
    }

    [[nodiscard]] RankedResults query_top_k(const std::string_view q, const CompleteResult &corpus,
                                            const std::size_t top_k = 0) const
    {
        return ResultRanker::rank(query_scores(q, corpus), top_k);
    }

private:
    Bm25Config config;
    Analyzer analyzer;
    Ingester ingester;
    Scorer scorer;
    ResultRanker ranker;
};

inline IngestResult ingest_text(const std::string_view text)
{
    return Analyzer::ingest_text(text);
}

inline CompleteResult ingest_multiple_texts(const std::vector<std::string_view> &texts)
{
    return Ingester::ingest_multiple_texts(texts);
}

inline score_vector query(const std::string_view q, const CompleteResult &res, const double k,
                          const double b)
{
    return Scorer::query_scores(q, res, Bm25Config{k, b});
}

inline RankedResults query_top_k(const std::string_view q, const CompleteResult &res,
                                 const std::size_t top_k,
                                 const Bm25Config config = Bm25Config::defaults())
{
    return ResultRanker::rank(Scorer::query_scores(q, res, config), top_k);
}
