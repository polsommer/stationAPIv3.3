#pragma once

#include <cstdint>
#include <string>
#include <vector>

class IDatabaseConnection;

struct SchemaValidationResult {
    int currentVersion = 0;
    int requiredVersion = 0;
    std::vector<std::string> pendingMigrations;
};

SchemaValidationResult ValidateDatabaseSchemaOrThrow(IDatabaseConnection& db);
