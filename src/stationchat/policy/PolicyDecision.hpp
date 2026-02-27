#pragma once

#include <string>

namespace policy {

enum class DecisionType {
    Allow,
    SoftWarn,
    Throttle,
    Block,
};

struct Decision {
    DecisionType type = DecisionType::Allow;
    int riskScore = 0;
    std::string reason;
};

inline const char* ToString(DecisionType type) {
    switch (type) {
    case DecisionType::Allow:
        return "allow";
    case DecisionType::SoftWarn:
        return "soft-warn";
    case DecisionType::Throttle:
        return "throttle";
    case DecisionType::Block:
        return "block";
    }

    return "allow";
}

} // namespace policy
