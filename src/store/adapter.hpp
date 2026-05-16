#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/types.hpp"

namespace bm25::Store
{

/**
 * @brief Storage backend interface for the BM25 index.
 *
 * IAdapter decouples the BM25 scoring algorithm from persistence. Implement
 * this interface to provide a custom backend (e.g. a host application's
 * database). The library ships with InMemory and SQLite adapters.
 *
 * ## Ownership and thread safety
 * The adapter owns the corpus state it manages. Implementations are not
 * required to be thread-safe; callers must synchronise concurrent access.
 *
 * ## Corpus consistency invariant
 * Every mutating method (UpsertDocument, DeleteDocument, Clear) must leave
 * CorpusStats and DocumentFrequencies in a consistent state before returning.
 * Scoring reads these values without additional locking.
 */
class Adapter
{
public:
    Adapter() = default;
    virtual ~Adapter() = default;

    Adapter(const Adapter &) = delete;
    Adapter &operator=(const Adapter &) = delete;
    Adapter(Adapter &&) = delete;
    Adapter &operator=(Adapter &&) = delete;

    /**
     * @brief Insert or replace a document by caller-supplied key.
     *
     * The key is an opaque, caller-managed identifier (e.g. a file path or
     * external UUID). The adapter assigns an internal @ref Core::DocumentId.
     *
     * - New key → stores the document and returns WriteOutcome::Inserted.
     * - Existing key → replaces the stored IngestResult in-place, adjusts
     *   corpus stats (df, avgdl, documentCount), and returns WriteOutcome::Updated.
     *
     * @param key    Caller-supplied unique identifier for the document.
     * @param result Pre-computed token count and per-term frequencies from the ingester.
     * @return WriteResult containing the assigned DocumentId and the write outcome.
     */
    [[nodiscard]] virtual Core::WriteResult UpsertDocument(std::string_view key,
                                                           const Core::IngestResult &result) = 0;

    /**
     * @brief Remove a document from the index by its internal ID.
     *
     * Corpus stats must be updated atomically with the removal so that
     * subsequent scoring reflects the correct document count and avgdl.
     *
     * @param id Internal DocumentId previously returned by UpsertDocument.
     * @return true if the document existed and was removed; false if not found.
     */
    virtual bool DeleteDocument(Core::DocumentId id) = 0;

    /**
     * @brief Remove all documents and reset corpus stats to zero.
     *
     * After Clear(), Stats() must return a zero-valued CorpusStats and
     * DocumentFrequencies() must return an empty map.
     */
    virtual void Clear() = 0;

    /**
     * @brief Return a snapshot of aggregate corpus statistics.
     *
     * Used by the scorer to compute normalised BM25 weights. The returned
     * value reflects the state at the time of the call; it is not kept live.
     *
     * @return CorpusStats with documentCount, totalTokens, and avgDocumentLength.
     */
    [[nodiscard]] virtual Core::CorpusStats Stats() const = 0;

    /**
     * @brief Return the document-frequency map for the entire corpus.
     *
     * Maps each indexed term to the number of documents that contain it.
     * Used by the scorer for IDF calculation. Returned by value; callers
     * should not hold references across mutations.
     *
     * @return DocFrequencyMap (term → document count).
     */
    [[nodiscard]] virtual Core::DocFrequencyMap DocumentFrequencies() const = 0;

    /**
     * @brief Fetch postings for a set of query terms.
     *
     * Returns every document that contains at least one of the given terms,
     * along with its IngestResult (token count + per-term frequencies). The
     * scorer uses this to compute per-document BM25 scores without a full
     * index scan.
     *
     * Documents that match none of the terms are omitted entirely.
     *
     * @param terms Query terms to look up (already tokenised and normalised).
     * @return Pairs of (DocumentId, IngestResult) for each matching document.
     */
    [[nodiscard]] virtual std::vector<std::pair<Core::DocumentId, Core::IngestResult>>
    FetchPostings(const std::vector<std::string> &terms) const = 0;
};

} // namespace Store
