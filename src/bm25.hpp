#pragma once

#include <expected>
#include <string_view>

#include "core/types.hpp"
#include "store/adapter.hpp"

namespace bm25
{
using DocumentId = Core::DocumentId;
using RankedResult = Core::RankedResult;
using RankedResults = Core::RankedResults;
using Bm25Params = Core::Bm25Params;
using Bm25Error = Core::Bm25Error;

class Bm25
{
public:
    explicit Bm25(Store::IAdapter &dataStore);

    [[nodiscard]] DocumentId IngestChunk(std::string_view chunk);
    void DeleteDocument(DocumentId id);

    // Returns all matching documents sorted by score descending.
    // Unknown terms and empty queries return an empty result (not an error).
    [[nodiscard]] std::expected<RankedResults, Bm25Error>
    Query(std::string_view query, Bm25Params params = {}) const;

    // Like Query but limits output to topK results (topK == 0 means no limit).
    [[nodiscard]] std::expected<RankedResults, Bm25Error>
    QueryTopK(std::string_view query, std::size_t topK, Bm25Params params = {}) const;

private:
    Store::IAdapter *store;
};
} // namespace bm25
