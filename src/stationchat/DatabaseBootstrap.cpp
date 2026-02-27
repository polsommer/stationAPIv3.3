#include "DatabaseBootstrap.hpp"

#include "Database.hpp"

#include <sstream>
#include <utility>

namespace {
constexpr int kRequiredSchemaVersion = 1;

std::vector<std::pair<int, std::string>> MigrationCatalogForBackend(const std::string& backend) {
    if (backend == "mariadb") {
        return {{1, "extras/migrations/mariadb/V001__baseline.sql"}};
    }

    throw DatabaseException(backend, 0, "unknown database backend for migration lookup");
}

bool TableExists(IDatabaseConnection& db, const std::string& tableName) {
    StatementHandle stmt{db.Prepare(
        "SELECT 1 FROM information_schema.tables WHERE table_schema = DATABASE() AND table_name = @table_name")};
    int tableNameIdx = stmt->BindParameterIndex("@table_name");
    stmt->BindText(tableNameIdx, tableName);
    return stmt->Step() == StatementStepResult::Row;
}

int ReadSchemaVersion(IDatabaseConnection& db) {
    StatementHandle stmt{db.Prepare("SELECT version FROM schema_version LIMIT 1")};
    if (stmt->Step() != StatementStepResult::Row) {
        throw DatabaseException(db.BackendName(), 0,
            "schema_version exists but has no rows. Apply baseline migration V001 before starting stationchat");
    }

    return stmt->ColumnInt(0);
}

std::string Join(const std::vector<std::string>& items) {
    std::ostringstream out;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << items[i];
    }
    return out.str();
}

} // namespace

SchemaValidationResult ValidateDatabaseSchemaOrThrow(IDatabaseConnection& db) {
    const auto migrations = MigrationCatalogForBackend(db.BackendName());
    const int latestKnownVersion = migrations.back().first;

    if (!TableExists(db, "schema_version")) {
        throw DatabaseException(db.BackendName(), 0,
            "schema_version table is missing. Apply baseline migration " + migrations.front().second
                + " and retry");
    }

    const int currentVersion = ReadSchemaVersion(db);

    if (currentVersion > latestKnownVersion) {
        throw DatabaseException(db.BackendName(), 0,
            "database schema version " + std::to_string(currentVersion)
                + " is newer than this binary supports (latest known migration: "
                + std::to_string(latestKnownVersion) + "). Deploy a newer stationchat binary");
    }

    SchemaValidationResult result;
    result.currentVersion = currentVersion;
    result.requiredVersion = kRequiredSchemaVersion;

    for (const auto& migration : migrations) {
        if (migration.first > currentVersion) {
            result.pendingMigrations.push_back(migration.second);
        }
    }

    if (currentVersion < kRequiredSchemaVersion) {
        throw DatabaseException(db.BackendName(), 0,
            "database schema version " + std::to_string(currentVersion) + " is below required "
                + std::to_string(kRequiredSchemaVersion) + ". Apply migrations: "
                + Join(result.pendingMigrations));
    }

    return result;
}
