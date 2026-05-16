#include "core/ingester.hpp"

#include <utility>

#include "utils/string_utils.hpp"

namespace bm25::Core
{
IngestResult Ingest(const std::string_view text)
{
    std::size_t tokenCount = 0;
    TermMap termFrequencies;

    tokenize(text, [&tokenCount, &termFrequencies](const std::string &token) {
        ++tokenCount;
        ++termFrequencies[token];
    });

    return {.tokenCount = tokenCount, .termFrequencies = std::move(termFrequencies)};
}
} // namespace bm25::Core
