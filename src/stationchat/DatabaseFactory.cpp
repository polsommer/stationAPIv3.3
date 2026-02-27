#include "DatabaseFactory.hpp"

#include "DatabaseBootstrap.hpp"
#if defined(STATIONCHAT_WITH_MARIADB)
#include "DatabaseMariaDb.hpp"
#endif
#if defined(STATIONCHAT_WITH_SQLITE)
#include "DatabaseSqlite.hpp"
#endif
#include "StationChatConfig.hpp"

#include "easylogging++.h"

#include <sstream>
#include <vector>

namespace {
std::string DescribeCapabilities(const DatabaseCapabilities& capabilities) {
    std::ostringstream out;

    out << "upsert=";
    switch (capabilities.upsertStrategy) {
    case UpsertStrategy::InsertIgnore:
        out << "INSERT IGNORE";
        break;
    case UpsertStrategy::InsertOrIgnore:
        out << "INSERT OR IGNORE";
        break;
    case UpsertStrategy::InsertOnConflictDoNothing:
        out << "ON CONFLICT DO NOTHING";
        break;
    }

    out << ", blob=";
    switch (capabilities.blobSemantics) {
    case BlobSemantics::NativeBlob:
        out << "native";
        break;
    case BlobSemantics::HexEncodedLiteral:
        out << "hex-literal";
        break;
    }

    out << ", tx_isolation=";
    switch (capabilities.transactionIsolationSupport) {
    case TransactionIsolationSupport::SerializableOnly:
        out << "serializable-only";
        break;
    case TransactionIsolationSupport::ReadCommitted:
        out << "read-committed";
        break;
    }

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

    if (config.databaseEngine == "mariadb") {
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
                + "; set these in swgchat.cfg (or pass --database_user/--database_schema). "
                + "To use legacy SQLite mode, set database_engine=sqlite and configure database_path");
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
    } else if (config.databaseEngine == "sqlite") {
#if defined(STATIONCHAT_WITH_SQLITE)
        connection = std::make_unique<SqliteDatabaseConnection>(config.chatDatabasePath);
#else
        throw DatabaseException("database", 0,
            "unsupported database_engine 'sqlite'; this binary was built without SQLite support");
#endif
    } else {
        throw DatabaseException("database", 0,
            "unsupported database_engine '" + config.databaseEngine + "'; expected sqlite or mariadb");
    }

    const auto validation = ValidateDatabaseSchemaOrThrow(*connection);
    LogSchemaStatus(*connection, validation);

    return connection;
}
