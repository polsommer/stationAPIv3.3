#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct NormalizedSql {
    std::string sql;
    std::unordered_map<std::string, int> logicalIndexByName;
    std::vector<std::vector<unsigned int>> positionsByLogicalIndex;
    std::vector<int> logicalIndexByPosition;
};

NormalizedSql NormalizeNamedParameters(const std::string& sql);

