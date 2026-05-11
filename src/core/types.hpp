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

// Error taxonomy.
//
// Recoverable — caller error; fix the arguments and retry.
//   InvalidParams: k <= 0 or b outside [0, 1].
//
// Fatal — internal invariant violated; the adapter is in an undefined state.
//   InternalError: reserved for storage-backed adapters (Milestone 1+).
//   Not reachable with the in-memory adapter.
enum class Bm25Error
{
    InvalidParams,
    InternalError,
};

// Outcome of a write (insert or update) operation.
enum class WriteOutcome
{
    Inserted, // key was new; a fresh DocumentId was assigned
    Updated,  // key already existed; the document was replaced in-place
};

// Return value of IngestChunk.
struct WriteResult
{
    DocumentId docId = 0;
    WriteOutcome outcome = WriteOutcome::Inserted;
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
