#pragma once

#include <cstddef>
#include <string>
#include <string_view>
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
    [[nodiscard]] Core::WriteResult UpsertDocument(const std::string_view key,
                                                   const Core::IngestResult &result) override
    {
        const auto keyIt = keyToId.find(std::string(key));

        if (keyIt != keyToId.end())
        {
            // Update path: remove old document's contributions before replacing.
            const auto id = keyIt->second;
            const auto &old = docs.at(id);
            totalTokens -= old.tokenCount;
            for (const auto &[term, _] : old.termFrequencies)
            {
                if (--spread[term] == 0)
                {
                    spread.erase(term);
                }
            }
            docs[id] = result;
            totalTokens += result.tokenCount;
            for (const auto &[term, _] : result.termFrequencies)
            {
                ++spread[term];
            }
            ++indexVersion;
            return {id, Core::WriteOutcome::Updated};
        }

        // Insert path: assign a new id.
        const auto id = nextId++;
        keyToId.emplace(key, id);
        idToKey.emplace(id, key);
        docs.emplace(id, result);
        totalTokens += result.tokenCount;
        for (const auto &[term, _] : result.termFrequencies)
        {
            ++spread[term];
        }
        ++indexVersion;
        return {id, Core::WriteOutcome::Inserted};
    }

    bool DeleteDocument(const Core::DocumentId id) override
    {
        const auto docIt = docs.find(id);
        if (docIt == docs.end())
        {
            return false;
        }

        totalTokens -= docIt->second.tokenCount;
        for (const auto &[term, _] : docIt->second.termFrequencies)
        {
            if (--spread[term] == 0)
            {
                spread.erase(term);
            }
        }
        keyToId.erase(idToKey.at(id));
        idToKey.erase(id);
        docs.erase(docIt);
        ++indexVersion;
        return true;
    }

    void Clear() override
    {
        docs.clear();
        keyToId.clear();
        idToKey.clear();
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
    std::unordered_map<std::string, Core::DocumentId> keyToId;
    std::unordered_map<Core::DocumentId, std::string> idToKey;
    std::unordered_map<Core::DocumentId, Core::IngestResult> docs;
    Core::DocFrequencyMap spread;
    std::size_t totalTokens = 0;
    Core::DocumentId nextId = 0;
    std::uint64_t indexVersion = 0;
};
} // namespace Store
