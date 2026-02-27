#include "SqlParameterAdapter.hpp"

namespace {

bool IsParameterNameCharacter(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '_';
}

} // namespace

NormalizedSql NormalizeNamedParameters(const std::string& sql) {
    NormalizedSql result;
    result.sql.reserve(sql.size());

    unsigned int positionalIndex = 0;

    for (size_t i = 0; i < sql.size();) {
        if (sql[i] != '@') {
            result.sql.push_back(sql[i]);
            ++i;
            continue;
        }

        const size_t start = i;
        ++i;
        while (i < sql.size() && IsParameterNameCharacter(sql[i])) {
            ++i;
        }

        const std::string name = sql.substr(start, i - start);

        auto existing = result.logicalIndexByName.find(name);
        int logicalIndex;
        if (existing == result.logicalIndexByName.end()) {
            logicalIndex = static_cast<int>(result.positionsByLogicalIndex.size());
            result.logicalIndexByName.emplace(name, logicalIndex);
            result.positionsByLogicalIndex.emplace_back();
        } else {
            logicalIndex = existing->second;
        }

        ++positionalIndex;
        result.positionsByLogicalIndex[logicalIndex].push_back(positionalIndex);
        result.logicalIndexByPosition.push_back(logicalIndex);
        result.sql.push_back('?');
    }

    return result;
}
