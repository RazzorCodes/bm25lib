#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/types.hpp"

namespace Store
{
using DocFrequencyMap = std::unordered_map<std::string, std::size_t>;

class IAdapter
{
public:
    virtual ~IAdapter() = default;

    virtual Core::DocumentId UpsertDocument(const Core::IngestResult &result) = 0;
    [[nodiscard]] virtual const std::vector<Core::IngestResult> &Documents() const = 0;
    [[nodiscard]] virtual DocFrequencyMap DocumentFrequencies() const = 0;
    [[nodiscard]] virtual Core::CorpusStats Stats() const = 0;
    virtual void Clear() = 0;
};
} // namespace Store
