#include "DatabaseMariaDb.hpp"

#include <stdexcept>

struct MariaDbDatabaseConnection::Impl {};

MariaDbDatabaseConnection::MariaDbDatabaseConnection(const std::string& host, uint16_t port,
    const std::string& user, const std::string& password, const std::string& schema)
    : impl_{std::make_unique<Impl>()} {
    (void)host;
    (void)port;
    (void)user;
    (void)password;
    (void)schema;
    throw DatabaseException("MariaDB provider is not enabled in this build. Install MariaDB Connector/C and extend build linking.");
}

MariaDbDatabaseConnection::~MariaDbDatabaseConnection() = default;

std::unique_ptr<IStatement> MariaDbDatabaseConnection::Prepare(const std::string& sql) {
    (void)sql;
    throw DatabaseException("MariaDB provider is not enabled in this build.");
}

std::unique_ptr<ITransaction> MariaDbDatabaseConnection::BeginTransaction() {
    throw DatabaseException("MariaDB provider is not enabled in this build.");
}

uint64_t MariaDbDatabaseConnection::GetLastInsertId() const {
    throw DatabaseException("MariaDB provider is not enabled in this build.");
}
