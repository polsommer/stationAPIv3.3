#pragma once

#include "Database.hpp"

#include <memory>

struct StationChatConfig;

std::unique_ptr<IDatabaseConnection> CreateDatabaseConnection(const StationChatConfig& config);
