#include "DatabaseSqlite.hpp"

#include "SqlParameterAdapter.hpp"

#include <sqlite3.h>

namespace {

DatabaseException MakeSqliteError(sqlite3* db, int code, const std::string& context) {
    const char* rawMessage = db ? sqlite3_errmsg(db) : nullptr;
    std::string message = rawMessage ? rawMessage : "unknown sqlite error";
    return DatabaseException("sqlite", code, context + ": " + message);
}

DatabaseException MakeSqliteError(int code, const std::string& context, const std::string& message) {
    return DatabaseException("sqlite", code, context + ": " + message);
}

class SqliteStatement final : public IStatement {
public:
    SqliteStatement(sqlite3* db, const std::string& sql)
        : db_{db}
        , stmt_{nullptr} {
        normalizedSql_ = NormalizeNamedParameters(sql);

        auto result = sqlite3_prepare_v2(db_, normalizedSql_.sql.c_str(), -1, &stmt_, nullptr);
        if (result != SQLITE_OK) {
            throw MakeSqliteError(db_, result, "prepare failed");
        }
    }

    ~SqliteStatement() override { sqlite3_finalize(stmt_); }

    int BindParameterIndex(const std::string& name) const override {
        auto iter = normalizedSql_.logicalIndexByName.find(name);
        if (iter == normalizedSql_.logicalIndexByName.end()) {
            throw DatabaseException("sqlite", SQLITE_MISUSE, "missing parameter: " + name);
        }

        return iter->second;
    }

    void BindInt(int index, int64_t value) override {
        for (auto position : Positions(index)) {
            if (sqlite3_bind_int64(stmt_, static_cast<int>(position), value) != SQLITE_OK) {
                throw MakeSqliteError(db_, sqlite3_errcode(db_), "bind int failed");
            }
        }
    }

    void BindText(int index, const std::string& value) override {
        for (auto position : Positions(index)) {
            if (sqlite3_bind_text(stmt_, static_cast<int>(position), value.c_str(), -1, SQLITE_TRANSIENT)
                != SQLITE_OK) {
                throw MakeSqliteError(db_, sqlite3_errcode(db_), "bind text failed");
            }
        }
    }

    void BindBlob(int index, const uint8_t* data, size_t length) override {
        for (auto position : Positions(index)) {
            if (sqlite3_bind_blob(stmt_, static_cast<int>(position), data, static_cast<int>(length), SQLITE_TRANSIENT)
                != SQLITE_OK) {
                throw MakeSqliteError(db_, sqlite3_errcode(db_), "bind blob failed");
            }
        }
    }

    StatementStepResult Step() override {
        auto result = sqlite3_step(stmt_);
        if (result == SQLITE_ROW) {
            return StatementStepResult::Row;
        }
        if (result == SQLITE_DONE) {
            return StatementStepResult::Done;
        }
        throw MakeSqliteError(db_, result, "step failed");
    }

    int ColumnInt(int index) const override { return sqlite3_column_int(stmt_, index); }

    std::string ColumnText(int index) const override {
        auto* text = sqlite3_column_text(stmt_, index);
        return text ? reinterpret_cast<const char*>(text) : "";
    }

    const uint8_t* ColumnBlob(int index) const override {
        return reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt_, index));
    }

    int ColumnBytes(int index) const override { return sqlite3_column_bytes(stmt_, index); }

private:
    const std::vector<unsigned int>& Positions(int index) const {
        if (index < 0 || static_cast<size_t>(index) >= normalizedSql_.positionsByLogicalIndex.size()) {
            throw DatabaseException("sqlite", SQLITE_MISUSE, "invalid bind index");
        }
        return normalizedSql_.positionsByLogicalIndex[static_cast<size_t>(index)];
    }

    sqlite3* db_;
    sqlite3_stmt* stmt_;
    NormalizedSql normalizedSql_;
};

class SqliteTransaction final : public ITransaction {
public:
    explicit SqliteTransaction(sqlite3* db)
        : db_{db}
        , done_{false} {
        Execute("BEGIN");
    }

    ~SqliteTransaction() override {
        if (!done_) {
            try {
                Rollback();
            } catch (...) {
            }
        }
    }

    void Commit() override {
        Execute("COMMIT");
        done_ = true;
    }

    void Rollback() override {
        Execute("ROLLBACK");
        done_ = true;
    }

private:
    void Execute(const char* sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string message = err ? err : "unknown sqlite error";
            sqlite3_free(err);
            throw MakeSqliteError(SQLITE_ERROR, "transaction failed", message);
        }
    }

    sqlite3* db_;
    bool done_;
};
} // namespace

SqliteDatabaseConnection::SqliteDatabaseConnection(const std::string& path)
    : db_{nullptr}
    , capabilities_{UpsertStrategy::InsertOrIgnore, BlobSemantics::NativeBlob, TransactionIsolationSupport::SerializableOnly} {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw MakeSqliteError(db_, sqlite3_errcode(db_), "open database failed");
    }
}

SqliteDatabaseConnection::~SqliteDatabaseConnection() { sqlite3_close(db_); }

std::unique_ptr<IStatement> SqliteDatabaseConnection::Prepare(const std::string& sql) {
    return std::make_unique<SqliteStatement>(db_, sql);
}

std::unique_ptr<ITransaction> SqliteDatabaseConnection::BeginTransaction() {
    return std::make_unique<SqliteTransaction>(db_);
}

uint64_t SqliteDatabaseConnection::GetLastInsertId() const {
    return static_cast<uint64_t>(sqlite3_last_insert_rowid(db_));
}

std::string SqliteDatabaseConnection::BackendName() const { return "sqlite"; }

const DatabaseCapabilities& SqliteDatabaseConnection::Capabilities() const { return capabilities_; }
