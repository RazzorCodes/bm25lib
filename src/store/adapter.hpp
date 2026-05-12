#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/types.hpp"

namespace Store
{
class IAdapter
{
public:
    virtual ~IAdapter() = default;

    IAdapter(const IAdapter &) = delete;
    IAdapter &operator=(const IAdapter &) = delete;
    IAdapter(IAdapter &&) = delete;
    IAdapter &operator=(IAdapter &&) = delete;

    // Upsert by caller-supplied key.
    //   - New key: creates a new document, returns WriteOutcome::Inserted.
    //   - Existing key: replaces the stored document, returns WriteOutcome::Updated.
    // Corpus stats (df, avgdl, documentCount) are kept consistent on both paths.
    [[nodiscard]] virtual Core::WriteResult UpsertDocument(std::string_view key,
                                                           const Core::IngestResult &result) = 0;

    // Removes the document identified by id.
    // Returns true if the document existed and was removed, false if id was not found.
    virtual bool DeleteDocument(Core::DocumentId id) = 0;

    virtual void Clear() = 0;

    [[nodiscard]] virtual Core::CorpusStats Stats() const = 0;
    [[nodiscard]] virtual Core::DocFrequencyMap DocumentFrequencies() const = 0;

    // Returns only documents that contain at least one of the given terms.
    [[nodiscard]] virtual std::vector<std::pair<Core::DocumentId, Core::IngestResult>>
    FetchPostings(const std::vector<std::string> &terms) const = 0;
};
} // namespace Store
