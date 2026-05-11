#pragma once

#include <string_view>

#include "core/types.hpp"

namespace Core
{
[[nodiscard]] IngestResult Ingest(std::string_view text);
} // namespace Core
