#include "DatabaseFactory.hpp"

#include "DatabaseBootstrap.hpp"
#if defined(STATIONCHAT_WITH_MARIADB)
#include "DatabaseMariaDb.hpp"
#endif
#include "StationChatConfig.hpp"

#include "easylogging++.h"

#include <sstream>
#include <vector>

namespace {
std::string DescribeCapabilities(const DatabaseCapabilities& capabilities) {
    std::ostringstream out;
    out << "upsert=INSERT IGNORE";
    out << ", blob="
        << (capabilities.blobSemantics == BlobSemantics::HexEncodedLiteral ? "hex-literal" : "native");
    out << ", tx_isolation="
        << (capabilities.transactionIsolationSupport == TransactionIsolationSupport::ReadCommitted
                ? "read-committed"
                : "serializable-only");
    return out.str();
}

std::string Join(const std::vector<std::string>& items) {
    if (items.empty()) {
        return "none";
    }

    std::ostringstream out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << items[i];
    }
    return out.str();
}

void LogSchemaStatus(const IDatabaseConnection& db, const SchemaValidationResult& result) {
    LOG(INFO) << "Database backend selected: " << db.BackendName();
    LOG(INFO) << "Database capabilities: " << DescribeCapabilities(db.Capabilities());
    LOG(INFO) << "Database schema version: " << result.currentVersion
              << " (required " << result.requiredVersion << ")";
    LOG(INFO) << "Required migrations before next version: " << Join(result.pendingMigrations);
}
} // namespace

std::unique_ptr<IDatabaseConnection> CreateDatabaseConnection(const StationChatConfig& config) {
    std::unique_ptr<IDatabaseConnection> connection;

    if (config.databaseEngine != "mariadb") {
        throw DatabaseException("database", 0,
            "unsupported database_engine '" + config.databaseEngine + "'; expected mariadb");
    }

#if defined(STATIONCHAT_WITH_MARIADB)
    std::vector<std::string> missing;
    if (config.databaseUser.empty()) {
        missing.emplace_back("database_user");
    }
    if (config.databaseSchema.empty()) {
        missing.emplace_back("database_schema");
    }

    if (!missing.empty()) {
        throw DatabaseException("database", 0,
            "database_engine=mariadb requires " + Join(missing)
            + "; set these in swgchat.cfg (or pass --database_user/--database_schema)");
    }

    connection = std::make_unique<MariaDbDatabaseConnection>(
        config.databaseHost,
        config.databasePort,
        config.databaseUser,
        config.databasePassword,
        config.databaseSchema);
#else
    throw DatabaseException("database", 0,
        "unsupported database_engine 'mariadb'; this binary was built without MariaDB support");
#endif

    const auto validation = ValidateDatabaseSchemaOrThrow(*connection);
    LogSchemaStatus(*connection, validation);

    return connection;
}
