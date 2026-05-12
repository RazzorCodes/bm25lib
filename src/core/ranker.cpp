#include "core/ranker.hpp"

#include <algorithm>

namespace Core
{
RankedResults RankScores(RankedResults scored, const std::size_t topK)
{
    std::ranges::sort(scored, [](const RankedResult &lhs, const RankedResult &rhs) {
        if (lhs.score != rhs.score)
        {
            return lhs.score > rhs.score;
        }

        return lhs.docId < rhs.docId;
    });

    if (topK > 0 && topK < scored.size())
    {
        scored.resize(topK);
    }

    return scored;
}
} // namespace Core
