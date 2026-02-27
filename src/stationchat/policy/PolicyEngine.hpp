#pragma once

#include "policy/PolicyDecision.hpp"
#include "policy/PolicyEvent.hpp"

#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>

struct StationChatConfig;

namespace policy {

class PolicyEngine {
public:
    explicit PolicyEngine(const StationChatConfig& config);

    Decision Evaluate(const Event& event);

private:
    using Clock = std::chrono::steady_clock;

    int CalculateRiskScore(const Event& event);
    int GetRecentActionCount(const Event& event, const Clock::time_point& now);
    std::string BuildRateKey(const Event& event) const;

    const StationChatConfig& config_;
    std::unordered_map<std::string, std::deque<Clock::time_point>> recentActions_;
};

} // namespace policy
