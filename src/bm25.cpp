#include "bm25.hpp"

#include <functional>

#include "core/ingester.hpp"
#include "core/ranker.hpp"
#include "core/scorer.hpp"
#include "utils/string_utils.hpp"

namespace bm25
{
Bm25::Bm25(Store::IAdapter &dataStore) : store(&dataStore) {}

DocumentId Bm25::IngestChunk(const std::string_view chunk)
{
    const auto id = std::hash<std::string_view>{}(chunk);
    store->UpsertDocument(id, Core::Ingest(chunk));
    return id;
}

void Bm25::DeleteDocument(const DocumentId id)
{
    store->DeleteDocument(id);
}

std::expected<RankedResults, Bm25Error>
Bm25::Query(const std::string_view query, const Bm25Params params) const
{
    if (!params.IsValid())
    {
        return std::unexpected(Core::Bm25Error::InvalidParams);
    }

    Core::TermMap queryTerms;
    tokenize(query, [&queryTerms](const std::string &token) { ++queryTerms[token]; });

    if (queryTerms.empty())
    {
        return RankedResults{};
    }

    std::vector<std::string> termList;
    termList.reserve(queryTerms.size());
    for (const auto &[term, _] : queryTerms)
    {
        termList.push_back(term);
    }

    const auto stats = store->Stats();
    const auto docFreqs = store->DocumentFrequencies();
    const auto postings = store->FetchPostings(termList);

    auto scored = Core::ScoreQuery(queryTerms, postings, stats, docFreqs, params);
    if (!scored)
    {
        return std::unexpected(scored.error());
    }

    return Core::RankScores(std::move(*scored), 0);
}

std::expected<RankedResults, Bm25Error>
Bm25::QueryTopK(const std::string_view query, const std::size_t topK,
                const Bm25Params params) const
{
    auto all = Query(query, params);
    if (!all)
    {
        return all;
    }

    if (topK > 0 && topK < all->size())
    {
        all->resize(topK);
    }

    return all;
}
} // namespace bm25
