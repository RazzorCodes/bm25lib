#pragma once

#include <expected>
#include <utility>
#include <vector>

#include "core/types.hpp"

namespace bm25::Core
{
// Internal: not part of the public API. Callers use bm25::Bm25::Query / QueryTopK.
//
// Scores only the provided postings against global corpus stats and doc-frequencies.
// queryTerms carries per-term query frequencies (queryTf). postings contains only
// documents that match at least one query term.
[[nodiscard]] std::expected<RankedResults, Bm25Error>
ScoreQuery(const TermMap &queryTerms,
           const std::vector<std::pair<DocumentId, IngestResult>> &postings,
           const CorpusStats &stats, const DocFrequencyMap &docFrequencies, Bm25Params params = {});
} // namespace bm25::Core
