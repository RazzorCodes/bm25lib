#include "core/scorer.hpp"

#include <cmath>

namespace bm25::Core
{
std::expected<RankedResults, Bm25Error>
ScoreQuery(const TermMap &queryTerms,
           const std::vector<std::pair<DocumentId, IngestResult>> &postings,
           const CorpusStats &stats, const DocFrequencyMap &docFrequencies, const Bm25Params params)
{
    if (!params.IsValid())
    {
        return std::unexpected(Bm25Error::InvalidParams);
    }

    RankedResults scored;
    if (queryTerms.empty() || postings.empty())
    {
        return scored;
    }

    const auto docCount = static_cast<double>(stats.documentCount);
    const auto avgdl = stats.avgDocumentLength;

    for (const auto &[docId, doc] : postings)
    {
        double score = 0.0;
        for (const auto &[term, queryTf] : queryTerms)
        {
            const auto dfIt = docFrequencies.find(term);
            if (dfIt == docFrequencies.end())
            {
                continue;
            }

            const auto tf = static_cast<double>([&]() -> std::size_t {
                const auto tfIt = doc.termFrequencies.find(term);
                return tfIt != doc.termFrequencies.end() ? tfIt->second : 0;
            }());
            if (tf == 0.0)
            {
                continue;
            }

            const auto df = static_cast<double>(dfIt->second);
            const auto idf = std::log(1.0 + ((docCount - df + 0.5) / (df + 0.5)));
            const auto lengthFactor =
                avgdl == 0.0
                    ? 1.0
                    : (1.0 - params.b + (params.b * static_cast<double>(doc.tokenCount) / avgdl));
            const auto den = tf + (params.k * lengthFactor);
            if (den == 0.0)
            {
                continue;
            }

            score += static_cast<double>(queryTf) * idf * (tf * (params.k + 1.0) / den);
        }
        scored.push_back({docId, score});
    }

    return scored;
}
} // namespace bm25::Core
