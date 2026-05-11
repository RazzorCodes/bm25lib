#pragma once

#include <expected>
#include <string_view>

#include "core/types.hpp"
#include "store/adapter.hpp"

namespace bm25
{
using DocumentId = Core::DocumentId;
using WriteOutcome = Core::WriteOutcome;
using WriteResult = Core::WriteResult;
using RankedResult = Core::RankedResult;
using RankedResults = Core::RankedResults;
using Bm25Params = Core::Bm25Params;
using Bm25Error = Core::Bm25Error;
using CorpusStats = Core::CorpusStats;

class Bm25
{
public:
    explicit Bm25(Store::IAdapter &dataStore);

    // Upsert by caller-supplied key.
    //   - New key: inserts the document; result.outcome == WriteOutcome::Inserted.
    //   - Existing key: replaces the document in-place; result.outcome == WriteOutcome::Updated.
    // The key is an opaque caller-owned string (path, URL, UUID, etc.). Two documents
    // with identical text but different keys are stored and scored independently.
    [[nodiscard]] WriteResult IngestChunk(std::string_view key, std::string_view text);

    // Removes the document identified by id.
    // Returns true if the document existed and was removed, false if id was not found.
    [[nodiscard]] bool DeleteDocument(DocumentId id);

    // Returns current corpus metadata: documentCount, totalTokens, avgDocumentLength,
    // and indexVersion. indexVersion increments on every insert, update, or delete.
    [[nodiscard]] CorpusStats Stats() const;

    // Returns all matching documents sorted by score descending.
    // Unknown terms and non-token queries return an empty result (not an error).
    // Returns Bm25Error::InvalidParams (recoverable) if params.IsValid() is false.
    [[nodiscard]] std::expected<RankedResults, Bm25Error>
    Query(std::string_view query, Bm25Params params = {}) const;

    // Like Query but limits output to topK results (topK == 0 means no limit).
    [[nodiscard]] std::expected<RankedResults, Bm25Error>
    QueryTopK(std::string_view query, std::size_t topK, Bm25Params params = {}) const;

private:
    Store::IAdapter *store;
};
} // namespace bm25
