#pragma once

#include <cstddef>

#include "core/types.hpp"

namespace bm25::Core
{
[[nodiscard]] RankedResults RankScores(RankedResults scored, std::size_t topK = 0);
} // namespace bm25::Core
