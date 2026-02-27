#pragma once

#include "Database.hpp"

class MariaDbDatabaseConnection final : public IDatabaseConnection {
public:
    MariaDbDatabaseConnection(const std::string& host, uint16_t port, const std::string& user,
        const std::string& password, const std::string& schema);
    ~MariaDbDatabaseConnection() override;

    std::unique_ptr<IStatement> Prepare(const std::string& sql) override;
    std::unique_ptr<ITransaction> BeginTransaction() override;
    uint64_t GetLastInsertId() const override;
    std::string BackendName() const override;
    const DatabaseCapabilities& Capabilities() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    DatabaseCapabilities capabilities_;
};
