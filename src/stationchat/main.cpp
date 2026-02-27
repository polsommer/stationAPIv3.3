
#define ELPP_DEFAULT_LOG_FILE "var/log/swgchat.log"
#include "easylogging++.h"

#include "StationChatApp.hpp"

#include <boost/program_options.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef __GNUC__
#include <execinfo.h>
#endif

INITIALIZE_EASYLOGGINGPP

StationChatConfig BuildConfiguration(int argc, const char* argv[]);

#ifdef __GNUC__
void SignalHandler(int sig);
#endif

int main(int argc, const char* argv[]) {
#ifdef __GNUC__
    signal(SIGSEGV, SignalHandler);
#endif

    auto config = BuildConfiguration(argc, argv);

    el::Loggers::setDefaultConfigurations(config.loggerConfig, true);
    START_EASYLOGGINGPP(argc, argv);

    StationChatApp app{config};

    while (app.IsRunning()) {
        app.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}

StationChatConfig BuildConfiguration(int argc, const char* argv[]) {
    namespace po = boost::program_options;
    StationChatConfig config;
    std::string configFile;

    auto ResolveDefaultPath = [](const std::vector<std::string>& candidatePaths) {
        for (const auto& candidatePath : candidatePaths) {
            std::ifstream candidate(candidatePath.c_str());
            if (candidate.good()) {
                return candidatePath;
            }
        }

        return candidatePaths.empty() ? std::string{} : candidatePaths.front();
    };

    // Declare a group of options that will be 
    // allowed only on command line
    po::options_description generic("Generic options");
    generic.add_options()
        ("help,h", "produces help message")
        ("config,c", po::value<std::string>(&configFile)->default_value("etc/stationapi/swgchat.cfg"),
            "sets path to the configuration file")
        ("logger_config", po::value<std::string>(&config.loggerConfig)->default_value("etc/stationapi/logger.cfg"),
            "sets path to the logger configuration file")
        ;

    po::options_description options("Configuration");
    options.add_options()
        ("gateway_address", po::value<std::string>(&config.gatewayAddress)->default_value("127.0.0.1"),
            "address for gateway connections")
        ("gateway_port", po::value<uint16_t>(&config.gatewayPort)->default_value(5001),
            "port for gateway connections")
        ("registrar_address", po::value<std::string>(&config.registrarAddress)->default_value("127.0.0.1"),
            "address for registrar connections")
        ("registrar_port", po::value<uint16_t>(&config.registrarPort)->default_value(5000),
            "port for registrar connections")
        ("bind_to_ip", po::value<bool>(&config.bindToIp)->default_value(false),
            "when set to true, binds to the config address; otherwise, binds on any interface")
        ("database_engine", po::value<std::string>(&config.databaseEngine)->default_value("mariadb"),
            "database engine (must be mariadb)")
        ("database_host", po::value<std::string>(&config.databaseHost)->default_value("127.0.0.1"),
            "database host (used when database_engine=mariadb)")
        ("database_port", po::value<uint16_t>(&config.databasePort)->default_value(3306),
            "database port (used when database_engine=mariadb)")
        ("database_user", po::value<std::string>(&config.databaseUser)->default_value(""),
            "database user (required when database_engine=mariadb)")
        ("database_password", po::value<std::string>(&config.databasePassword)->default_value(""),
            "database password (used when database_engine=mariadb)")
        ("database_schema", po::value<std::string>(&config.databaseSchema)->default_value(""),
            "database schema (required when database_engine=mariadb)")
        ;

    po::options_description cmdline_options;
    cmdline_options.add(generic).add(options);

    po::options_description config_file_options;
    config_file_options.add(options);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(cmdline_options).allow_unregistered().run(), vm);
    po::notify(vm);

    if (vm["config"].defaulted()) {
        configFile = ResolveDefaultPath({"swgchat.cfg", "stationchat.cfg", configFile});
    }
    if (vm["logger_config"].defaulted()) {
        config.loggerConfig = ResolveDefaultPath({"logger.cfg", config.loggerConfig});
    }

    std::ifstream ifs(configFile.c_str());
    if (!ifs) {
        throw std::runtime_error("Cannot open configuration file: " + configFile);
    }

    po::store(po::parse_config_file(ifs, config_file_options), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << cmdline_options << "\n";
        exit(EXIT_SUCCESS);
    }

    return config;
}

#ifdef __GNUC__
void SignalHandler(int sig) {
    const int BACKTRACE_LIMIT = 10;
    void *arr[BACKTRACE_LIMIT];
    auto size = backtrace(arr, BACKTRACE_LIMIT);

    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(arr, size, STDERR_FILENO);
    exit(1);
}
#endif
