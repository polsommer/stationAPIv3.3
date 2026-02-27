#pragma once

#include <cstdint>
#include <string>

struct StationChatConfig {
    StationChatConfig() = default;

    const uint32_t version = 2;
    std::string gatewayAddress;
    uint16_t gatewayPort;
    std::string registrarAddress;
    uint16_t registrarPort;

    // MariaDB is the default backend. Set databaseEngine="sqlite" to use legacy file-backed mode.
    std::string databaseEngine = "mariadb";
    std::string chatDatabasePath;
    std::string databaseHost = "127.0.0.1";
    uint16_t databasePort = 3306;
    std::string databaseUser;
    std::string databasePassword;
    std::string databaseSchema;

    std::string loggerConfig;
    bool bindToIp;
};
