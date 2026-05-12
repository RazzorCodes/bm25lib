#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include "core/types.hpp"
#include "store/adapter.hpp"

namespace Store
{
class Sqlite final : public IAdapter
{
public:
    explicit Sqlite(std::string path = ":memory:") : path_(std::move(path))
    {
        Open();
        InitializeSchema();
    }

    ~Sqlite() override
    {
        if (db_ != nullptr)
        {
            (void)sqlite3_close_v2(db_);
        }
    }

    Sqlite(const Sqlite &) = delete;
    Sqlite &operator=(const Sqlite &) = delete;
    Sqlite(Sqlite &&) = delete;
    Sqlite &operator=(Sqlite &&) = delete;

    [[nodiscard]] Core::WriteResult UpsertDocument(const std::string_view key,
                                                   const Core::IngestResult &result) override
    {
        Transaction txn(*this);

        const auto existing = FindDocumentByKey(key);
        if (existing.has_value())
        {
            const auto [id, oldTokenCount] = *existing;
            UpdateDocument(id, result);
            AdjustStoreState(ToSqlInt(result.tokenCount) - oldTokenCount, 1);
            txn.Commit();
            return {id, Core::WriteOutcome::Updated};
        }

        const auto id = InsertDocument(key, result.tokenCount);
        InsertTermFrequencies(id, result.termFrequencies);
        AdjustStoreState(ToSqlInt(result.tokenCount), 1);
        txn.Commit();
        return {id, Core::WriteOutcome::Inserted};
    }

    bool DeleteDocument(const Core::DocumentId id) override
    {
        Transaction txn(*this);

        const auto oldTokenCount = FindTokenCountById(id);
        if (!oldTokenCount.has_value())
        {
            return false;
        }

        auto statement = Prepare("DELETE FROM documents WHERE id = ?1;");
        BindInt64(statement.get(), 1, ToSqlInt(id));
        StepDone(statement.get(), "delete document");

        AdjustStoreState(-(*oldTokenCount), 1);
        txn.Commit();
        return true;
    }

    void Clear() override
    {
        Transaction txn(*this);
        Exec("DELETE FROM documents;");
        auto statement = Prepare(
            "UPDATE store_state SET total_tokens = 0, index_version = index_version + 1 "
            "WHERE singleton = 1;");
        StepDone(statement.get(), "clear store state");
        txn.Commit();
    }

    [[nodiscard]] Core::CorpusStats Stats() const override
    {
        auto statement = Prepare(
            "SELECT "
            "    (SELECT COUNT(*) FROM documents), "
            "    total_tokens, "
            "    index_version "
            "FROM store_state "
            "WHERE singleton = 1;");

        if (sqlite3_step(statement.get()) != SQLITE_ROW)
        {
            throw std::runtime_error("sqlite store: failed to fetch corpus stats");
        }

        Core::CorpusStats stats;
        stats.documentCount = FromSqlSize(sqlite3_column_int64(statement.get(), 0), "document count");
        stats.totalTokens = FromSqlSize(sqlite3_column_int64(statement.get(), 1), "total tokens");
        stats.indexVersion =
            FromSqlUint64(sqlite3_column_int64(statement.get(), 2), "index version");
        stats.avgDocumentLength =
            stats.documentCount == 0
                ? 0.0
                : static_cast<double>(stats.totalTokens) / static_cast<double>(stats.documentCount);
        return stats;
    }

    [[nodiscard]] Core::DocFrequencyMap DocumentFrequencies() const override
    {
        auto statement =
            Prepare("SELECT term, COUNT(*) FROM term_frequencies GROUP BY term;");

        Core::DocFrequencyMap frequencies;
        while (true)
        {
            const int rc = sqlite3_step(statement.get());
            if (rc == SQLITE_DONE)
            {
                break;
            }
            if (rc != SQLITE_ROW)
            {
                ThrowLastError("fetch document frequencies");
            }

            const auto *termText = sqlite3_column_text(statement.get(), 0);
            if (termText == nullptr)
            {
                throw std::runtime_error("sqlite store: term frequency row had null term");
            }

            frequencies.emplace(reinterpret_cast<const char *>(termText),
                                FromSqlSize(sqlite3_column_int64(statement.get(), 1),
                                            "document frequency"));
        }

        return frequencies;
    }

    [[nodiscard]] std::vector<std::pair<Core::DocumentId, Core::IngestResult>>
    FetchPostings(const std::vector<std::string> &terms) const override
    {
        if (terms.empty())
        {
            return {};
        }

        const auto placeholders = MakePlaceholders(terms.size());
        const auto joinedPlaceholders = MakePlaceholders(terms.size(), terms.size() + 1);
        auto statement = Prepare(
            "SELECT docs.id, docs.token_count, tf.term, tf.frequency "
            "FROM documents AS docs "
            "JOIN ("
            "    SELECT DISTINCT document_id "
            "    FROM term_frequencies "
            "    WHERE term IN (" +
            placeholders +
            ")) AS matched ON matched.document_id = docs.id "
            "LEFT JOIN term_frequencies AS tf "
            "    ON tf.document_id = docs.id AND tf.term IN (" +
            joinedPlaceholders +
            ") "
            "ORDER BY docs.id, tf.term;");

        BindTerms(statement.get(), terms, 1);
        BindTerms(statement.get(), terms, terms.size() + 1);

        std::vector<std::pair<Core::DocumentId, Core::IngestResult>> postings;
        Core::DocumentId currentId = 0;
        bool haveCurrent = false;

        while (true)
        {
            const int rc = sqlite3_step(statement.get());
            if (rc == SQLITE_DONE)
            {
                break;
            }
            if (rc != SQLITE_ROW)
            {
                ThrowLastError("fetch postings");
            }

            const auto docId = FromSqlSize(sqlite3_column_int64(statement.get(), 0), "document id");
            if (!haveCurrent || docId != currentId)
            {
                currentId = docId;
                haveCurrent = true;
                postings.emplace_back(
                    currentId,
                    Core::IngestResult{
                        .tokenCount =
                            FromSqlSize(sqlite3_column_int64(statement.get(), 1), "token count"),
                        .termFrequencies = {},
                    });
            }

            const auto *termText = sqlite3_column_text(statement.get(), 2);
            if (termText != nullptr)
            {
                postings.back().second.termFrequencies.emplace(
                    reinterpret_cast<const char *>(termText),
                    FromSqlSize(sqlite3_column_int64(statement.get(), 3), "term frequency"));
            }
        }

        return postings;
    }

private:
    class StatementDeleter
    {
    public:
        void operator()(sqlite3_stmt *statement) const noexcept
        {
            if (statement != nullptr)
            {
                (void)sqlite3_finalize(statement);
            }
        }
    };

    using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

    class Transaction
    {
    public:
        explicit Transaction(Sqlite &store) : store_(store)
        {
            store_.Exec("BEGIN IMMEDIATE TRANSACTION;");
        }

        ~Transaction()
        {
            if (!committed_)
            {
                (void)sqlite3_exec(store_.db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            }
        }

        void Commit()
        {
            store_.Exec("COMMIT;");
            committed_ = true;
        }

        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;

    private:
        Sqlite &store_;
        bool committed_ = false;
    };

    void Open()
    {
        const int rc = sqlite3_open_v2(path_.c_str(), &db_,
                                       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                           SQLITE_OPEN_FULLMUTEX,
                                       nullptr);
        if (rc != SQLITE_OK)
        {
            const std::string message =
                db_ != nullptr ? sqlite3_errmsg(db_) : "unable to open database";
            if (db_ != nullptr)
            {
                (void)sqlite3_close_v2(db_);
                db_ = nullptr;
            }
            throw std::runtime_error("sqlite store: " + message);
        }

        Exec("PRAGMA foreign_keys = ON;");
        if (!IsInMemoryPath())
        {
            Exec("PRAGMA journal_mode = WAL;");
        }
    }

    void InitializeSchema()
    {
        Exec(
            "CREATE TABLE IF NOT EXISTS store_state ("
            "    singleton INTEGER PRIMARY KEY CHECK (singleton = 1),"
            "    total_tokens INTEGER NOT NULL DEFAULT 0,"
            "    index_version INTEGER NOT NULL DEFAULT 0"
            ");");
        Exec(
            "INSERT INTO store_state (singleton, total_tokens, index_version) "
            "VALUES (1, 0, 0) "
            "ON CONFLICT(singleton) DO NOTHING;");
        Exec(
            "CREATE TABLE IF NOT EXISTS documents ("
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    doc_key TEXT NOT NULL UNIQUE,"
            "    token_count INTEGER NOT NULL"
            ");");
        Exec(
            "CREATE TABLE IF NOT EXISTS term_frequencies ("
            "    document_id INTEGER NOT NULL,"
            "    term TEXT NOT NULL,"
            "    frequency INTEGER NOT NULL,"
            "    PRIMARY KEY (document_id, term),"
            "    FOREIGN KEY (document_id) REFERENCES documents(id) ON DELETE CASCADE"
            ");");
        Exec("CREATE INDEX IF NOT EXISTS idx_term_frequencies_term ON term_frequencies(term);");
    }

    [[nodiscard]] StatementPtr Prepare(const std::string &sql) const
    {
        sqlite3_stmt *statement = nullptr;
        const int rc = sqlite3_prepare_v2(db_, sql.c_str(), static_cast<int>(sql.size()),
                                          &statement, nullptr);
        if (rc != SQLITE_OK)
        {
            ThrowLastError("prepare statement");
        }
        return StatementPtr(statement);
    }

    void Exec(const char *sql) const
    {
        char *errorMessage = nullptr;
        const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errorMessage);
        if (rc != SQLITE_OK)
        {
            std::string message =
                errorMessage != nullptr ? errorMessage : sqlite3_errmsg(db_);
            if (errorMessage != nullptr)
            {
                sqlite3_free(errorMessage);
            }
            throw std::runtime_error("sqlite store: " + message);
        }
    }

    static void BindInt64(sqlite3_stmt *statement, const int index, const sqlite3_int64 value)
    {
        if (sqlite3_bind_int64(statement, index, value) != SQLITE_OK)
        {
            throw std::runtime_error("sqlite store: failed to bind integer parameter");
        }
    }

    static void BindText(sqlite3_stmt *statement, const int index, const std::string_view value)
    {
        if (sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                              SQLITE_TRANSIENT) != SQLITE_OK)
        {
            throw std::runtime_error("sqlite store: failed to bind text parameter");
        }
    }

    void StepDone(sqlite3_stmt *statement, const char *context) const
    {
        const int rc = sqlite3_step(statement);
        if (rc != SQLITE_DONE)
        {
            ThrowLastError(context);
        }
    }

    [[nodiscard]] std::optional<std::pair<Core::DocumentId, sqlite3_int64>>
    FindDocumentByKey(const std::string_view key) const
    {
        auto statement =
            Prepare("SELECT id, token_count FROM documents WHERE doc_key = ?1;");
        BindText(statement.get(), 1, key);

        const int rc = sqlite3_step(statement.get());
        if (rc == SQLITE_DONE)
        {
            return std::nullopt;
        }
        if (rc != SQLITE_ROW)
        {
            ThrowLastError("lookup document by key");
        }

        return std::make_pair(
            FromSqlSize(sqlite3_column_int64(statement.get(), 0), "document id"),
            sqlite3_column_int64(statement.get(), 1));
    }

    [[nodiscard]] std::optional<sqlite3_int64> FindTokenCountById(const Core::DocumentId id) const
    {
        auto statement =
            Prepare("SELECT token_count FROM documents WHERE id = ?1;");
        BindInt64(statement.get(), 1, ToSqlInt(id));

        const int rc = sqlite3_step(statement.get());
        if (rc == SQLITE_DONE)
        {
            return std::nullopt;
        }
        if (rc != SQLITE_ROW)
        {
            ThrowLastError("lookup token count by id");
        }

        return sqlite3_column_int64(statement.get(), 0);
    }

    [[nodiscard]] Core::DocumentId InsertDocument(const std::string_view key,
                                                  const std::size_t tokenCount)
    {
        auto statement =
            Prepare("INSERT INTO documents (doc_key, token_count) VALUES (?1, ?2);");
        BindText(statement.get(), 1, key);
        BindInt64(statement.get(), 2, ToSqlInt(tokenCount));
        StepDone(statement.get(), "insert document");
        return FromSqlSize(sqlite3_last_insert_rowid(db_), "inserted document id");
    }

    void UpdateDocument(const Core::DocumentId id, const Core::IngestResult &result)
    {
        auto updateStatement =
            Prepare("UPDATE documents SET token_count = ?2 WHERE id = ?1;");
        BindInt64(updateStatement.get(), 1, ToSqlInt(id));
        BindInt64(updateStatement.get(), 2, ToSqlInt(result.tokenCount));
        StepDone(updateStatement.get(), "update document");

        auto deleteTerms =
            Prepare("DELETE FROM term_frequencies WHERE document_id = ?1;");
        BindInt64(deleteTerms.get(), 1, ToSqlInt(id));
        StepDone(deleteTerms.get(), "delete existing term frequencies");

        InsertTermFrequencies(id, result.termFrequencies);
    }

    void InsertTermFrequencies(const Core::DocumentId id, const Core::TermMap &frequencies)
    {
        if (frequencies.empty())
        {
            return;
        }

        auto statement = Prepare(
            "INSERT INTO term_frequencies (document_id, term, frequency) VALUES (?1, ?2, ?3);");
        for (const auto &[term, frequency] : frequencies)
        {
            if (sqlite3_reset(statement.get()) != SQLITE_OK)
            {
                ThrowLastError("reset term insert statement");
            }
            if (sqlite3_clear_bindings(statement.get()) != SQLITE_OK)
            {
                ThrowLastError("clear term insert statement");
            }

            BindInt64(statement.get(), 1, ToSqlInt(id));
            BindText(statement.get(), 2, term);
            BindInt64(statement.get(), 3, ToSqlInt(frequency));
            StepDone(statement.get(), "insert term frequency");
        }
    }

    // index_version mirrors the in-memory adapter: every successful store mutation increments once.
    void AdjustStoreState(const sqlite3_int64 tokenDelta, const sqlite3_int64 versionDelta)
    {
        auto statement = Prepare(
            "UPDATE store_state "
            "SET total_tokens = total_tokens + ?1, index_version = index_version + ?2 "
            "WHERE singleton = 1;");
        BindInt64(statement.get(), 1, tokenDelta);
        BindInt64(statement.get(), 2, versionDelta);
        StepDone(statement.get(), "update store state");
    }

    void BindTerms(sqlite3_stmt *statement, const std::vector<std::string> &terms,
                   const std::size_t startIndex) const
    {
        for (std::size_t index = 0; index < terms.size(); ++index)
        {
            BindText(statement, static_cast<int>(startIndex + index), terms[index]);
        }
    }

    [[nodiscard]] static std::string MakePlaceholders(const std::size_t count,
                                                      const std::size_t startIndex = 1)
    {
        std::string placeholders;
        placeholders.reserve(count * 6);
        for (std::size_t index = 0; index < count; ++index)
        {
            if (index != 0)
            {
                placeholders += ", ";
            }
            placeholders += '?';
            placeholders += std::to_string(startIndex + index);
        }
        return placeholders;
    }

    [[nodiscard]] bool IsInMemoryPath() const
    {
        return path_ == ":memory:";
    }

    [[noreturn]] void ThrowLastError(const char *context) const
    {
        throw std::runtime_error(std::string("sqlite store: ") + context + ": " +
                                 sqlite3_errmsg(db_));
    }

    [[nodiscard]] static sqlite3_int64 ToSqlInt(const std::size_t value)
    {
        if (value > static_cast<std::size_t>(std::numeric_limits<sqlite3_int64>::max()))
        {
            throw std::overflow_error("sqlite store: integer value exceeds sqlite range");
        }
        return static_cast<sqlite3_int64>(value);
    }

    [[nodiscard]] static std::size_t FromSqlSize(const sqlite3_int64 value, const char *field)
    {
        if (value < 0)
        {
            throw std::runtime_error(std::string("sqlite store: negative value for ") + field);
        }
        if (static_cast<unsigned long long>(value) >
            static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()))
        {
            throw std::overflow_error(std::string("sqlite store: value for ") + field +
                                      " exceeds size_t range");
        }
        return static_cast<std::size_t>(value);
    }

    [[nodiscard]] static std::uint64_t FromSqlUint64(const sqlite3_int64 value, const char *field)
    {
        if (value < 0)
        {
            throw std::runtime_error(std::string("sqlite store: negative value for ") + field);
        }
        return static_cast<std::uint64_t>(value);
    }

    std::string path_;
    sqlite3 *db_ = nullptr;
};
} // namespace Store
