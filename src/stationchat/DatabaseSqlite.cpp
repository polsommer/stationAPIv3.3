#include "DatabaseSqlite.hpp"

#include <sqlite3.h>

namespace {
class SqliteStatement final : public IStatement {
public:
    SqliteStatement(sqlite3* db, const std::string& sql)
        : db_{db}
        , stmt_{nullptr} {
        auto result = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr);
        if (result != SQLITE_OK) {
            throw DatabaseException("sqlite prepare failed: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    ~SqliteStatement() override { sqlite3_finalize(stmt_); }

    int BindParameterIndex(const std::string& name) const override {
        return sqlite3_bind_parameter_index(stmt_, name.c_str());
    }

    void BindInt(int index, int64_t value) override {
        if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
            throw DatabaseException("sqlite bind int failed: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    void BindText(int index, const std::string& value) override {
        if (sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            throw DatabaseException("sqlite bind text failed: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    void BindBlob(int index, const uint8_t* data, size_t length) override {
        if (sqlite3_bind_blob(stmt_, index, data, static_cast<int>(length), SQLITE_TRANSIENT) != SQLITE_OK) {
            throw DatabaseException("sqlite bind blob failed: " + std::string(sqlite3_errmsg(db_)));
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
        throw DatabaseException("sqlite step failed: " + std::string(sqlite3_errmsg(db_)));
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
    sqlite3* db_;
    sqlite3_stmt* stmt_;
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
            throw DatabaseException("sqlite transaction failed: " + message);
        }
    }

    sqlite3* db_;
    bool done_;
};
} // namespace

SqliteDatabaseConnection::SqliteDatabaseConnection(const std::string& path)
    : db_{nullptr} {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        throw DatabaseException("Can't open sqlite database: " + std::string(sqlite3_errmsg(db_)));
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
