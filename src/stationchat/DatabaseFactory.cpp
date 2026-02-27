#include "DatabaseFactory.hpp"

#include "DatabaseMariaDb.hpp"
#include "DatabaseSqlite.hpp"
#include "StationChatConfig.hpp"

std::unique_ptr<IDatabaseConnection> CreateDatabaseConnection(const StationChatConfig& config) {
    if (config.databaseEngine == "mariadb") {
        return std::make_unique<MariaDbDatabaseConnection>(
            config.databaseHost,
            config.databasePort,
            config.databaseUser,
            config.databasePassword,
            config.databaseSchema);
    }

    return std::make_unique<SqliteDatabaseConnection>(config.chatDatabasePath);
}
