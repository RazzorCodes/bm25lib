#pragma once

#include "utils/string_utils.hpp"

#include "core/types.hpp"

namespace Core
{
inline IngestResult Ingest(std::string_view text)
{
    std::size_t n_token = 0;
    TermMap tf;

    tokenize(text, [&n_token, &tf](const std::string &token) {
        ++n_token;
        ++tf[token];
    });

    return {n_token, std::move(tf)};
}
} // namespace Core
