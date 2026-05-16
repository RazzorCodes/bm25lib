#pragma once

#include <string_view>

#include "core/types.hpp"

namespace bm25::Core
{
[[nodiscard]] IngestResult Ingest(std::string_view text);
} // namespace bm25::Core
