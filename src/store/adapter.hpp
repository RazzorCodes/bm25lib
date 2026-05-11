#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/types.hpp"

namespace Store
{
class IAdapter
{
public:
    virtual ~IAdapter() = default;

    // Idempotent by id: repeated calls with the same id replace the stored entry.
    virtual void UpsertDocument(Core::DocumentId id, const Core::IngestResult &result) = 0;
    virtual void DeleteDocument(Core::DocumentId id) = 0;
    virtual void Clear() = 0;

    [[nodiscard]] virtual Core::CorpusStats Stats() const = 0;
    [[nodiscard]] virtual Core::DocFrequencyMap DocumentFrequencies() const = 0;

    // Returns only documents that contain at least one of the given terms.
    [[nodiscard]] virtual std::vector<std::pair<Core::DocumentId, Core::IngestResult>>
    FetchPostings(const std::vector<std::string> &terms) const = 0;
};
} // namespace Store
