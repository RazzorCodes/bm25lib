#pragma once

#include <cstddef>
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
    Core::DocumentId UpsertDocument(const Core::IngestResult &result) override
    {
        const auto docId = corpus.size();
        corpus.push_back(result);
        totalTokens += result.tokenCount;

        for (const auto &[term, _] : result.termFrequencies)
        {
            ++spread[term];
        }

        return docId;
    }

    [[nodiscard]] const std::vector<Core::IngestResult> &Documents() const override
    {
        return corpus;
    }

    [[nodiscard]] DocFrequencyMap DocumentFrequencies() const override
    {
        return spread;
    }

    [[nodiscard]] Core::CorpusStats Stats() const override
    {
        Core::CorpusStats stats;
        stats.documentCount = corpus.size();
        stats.totalTokens = totalTokens;
        stats.avgDocumentLength =
            stats.documentCount == 0
                ? 0.0
                : static_cast<double>(totalTokens) / static_cast<double>(stats.documentCount);
        return stats;
    }

    void Clear() override
    {
        corpus.clear();
        spread.clear();
        totalTokens = 0;
    }

private:
    std::vector<Core::IngestResult> corpus;
    DocFrequencyMap spread;
    std::size_t totalTokens = 0;
};
} // namespace Store
