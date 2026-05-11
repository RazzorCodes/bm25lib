#pragma once

#include <cstddef>

#include "core/types.hpp"

namespace Core
{
[[nodiscard]] RankedResults RankScores(RankedResults scored, std::size_t topK = 0);
} // namespace Core
