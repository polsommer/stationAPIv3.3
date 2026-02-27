#pragma once

#include "Database.hpp"

struct sqlite3;
struct sqlite3_stmt;

class SqliteDatabaseConnection final : public IDatabaseConnection {
public:
    explicit SqliteDatabaseConnection(const std::string& path);
    ~SqliteDatabaseConnection() override;

    std::unique_ptr<IStatement> Prepare(const std::string& sql) override;
    std::unique_ptr<ITransaction> BeginTransaction() override;
    uint64_t GetLastInsertId() const override;

    sqlite3* GetNativeHandle() const { return db_; }

private:
    sqlite3* db_;
};
