#pragma once

#include "Database.hpp"

class MariaDbDatabaseConnection final : public IDatabaseConnection {
public:
    MariaDbDatabaseConnection(const std::string& host, uint16_t port, const std::string& user,
        const std::string& password, const std::string& schema, const std::string& sslMode,
        const std::string& sslCa, const std::string& sslCaPath, const std::string& sslCert,
        const std::string& sslKey);
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
