#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Core
{
using DocumentId = std::size_t;
using TermFrequency = std::size_t;
using TermMap = std::unordered_map<std::string, TermFrequency>;
using DocFrequencyMap = std::unordered_map<std::string, std::size_t>;

enum class Bm25Error
{
    InvalidParams,
};

struct IngestResult
{
    std::size_t tokenCount = 0;
    TermMap termFrequencies;
};

struct CorpusStats
{
    std::size_t documentCount = 0;
    std::size_t totalTokens = 0;
    double avgDocumentLength = 0.0;
    std::uint64_t indexVersion = 0;
};

struct RankedResult
{
    DocumentId docId = 0;
    double score = 0.0;
};

using RankedResults = std::vector<RankedResult>;

struct Bm25Params
{
    double k = 1.2;
    double b = 0.75;

    [[nodiscard]] bool IsValid() const
    {
        return k > 0.0 && b >= 0.0 && b <= 1.0;
    }
};
} // namespace Core
