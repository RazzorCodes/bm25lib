#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/types.hpp"
#include "store/adapter.hpp"

namespace Store
{
class InMemory final : public IAdapter
{
public:
    void UpsertDocument(const Core::DocumentId id, const Core::IngestResult &result) override
    {
        const auto existing = docs.find(id);
        if (existing != docs.end())
        {
            // Remove old document's contributions before replacing.
            totalTokens -= existing->second.tokenCount;
            for (const auto &[term, _] : existing->second.termFrequencies)
            {
                if (--spread[term] == 0)
                {
                    spread.erase(term);
                }
            }
        }

        docs[id] = result;
        totalTokens += result.tokenCount;
        for (const auto &[term, _] : result.termFrequencies)
        {
            ++spread[term];
        }
        ++indexVersion;
    }

    void DeleteDocument(const Core::DocumentId id) override
    {
        const auto it = docs.find(id);
        if (it == docs.end())
        {
            return;
        }

        totalTokens -= it->second.tokenCount;
        for (const auto &[term, _] : it->second.termFrequencies)
        {
            if (--spread[term] == 0)
            {
                spread.erase(term);
            }
        }
        docs.erase(it);
        ++indexVersion;
    }

    void Clear() override
    {
        docs.clear();
        spread.clear();
        totalTokens = 0;
        ++indexVersion;
    }

    [[nodiscard]] Core::CorpusStats Stats() const override
    {
        Core::CorpusStats stats;
        stats.documentCount = docs.size();
        stats.totalTokens = totalTokens;
        stats.avgDocumentLength =
            stats.documentCount == 0
                ? 0.0
                : static_cast<double>(totalTokens) / static_cast<double>(stats.documentCount);
        stats.indexVersion = indexVersion;
        return stats;
    }

    [[nodiscard]] Core::DocFrequencyMap DocumentFrequencies() const override
    {
        return spread;
    }

    [[nodiscard]] std::vector<std::pair<Core::DocumentId, Core::IngestResult>>
    FetchPostings(const std::vector<std::string> &terms) const override
    {
        std::vector<std::pair<Core::DocumentId, Core::IngestResult>> result;
        for (const auto &[docId, doc] : docs)
        {
            for (const auto &term : terms)
            {
                if (doc.termFrequencies.count(term) > 0)
                {
                    result.emplace_back(docId, doc);
                    break;
                }
            }
        }
        return result;
    }

private:
    std::unordered_map<Core::DocumentId, Core::IngestResult> docs;
    Core::DocFrequencyMap spread;
    std::size_t totalTokens = 0;
    std::uint64_t indexVersion = 0;
};
} // namespace Store
