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

    std::string databaseEngine = "mariadb";
    std::string databaseHost = "127.0.0.1";
    uint16_t databasePort = 3306;
    std::string databaseUser;
    std::string databasePassword;
    std::string databaseSchema;
    std::string databaseSslMode;
    std::string databaseSslCa;
    std::string databaseSslCaPath;
    std::string databaseSslCert;
    std::string databaseSslKey;

    std::string loggerConfig;
    bool bindToIp;
};
