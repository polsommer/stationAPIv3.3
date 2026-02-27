#include "policy/PolicyEngine.hpp"

#include "StationChatConfig.hpp"

#include <sstream>

namespace policy {

namespace {
constexpr int RATE_WINDOW_SECONDS = 10;
}

PolicyEngine::PolicyEngine(const StationChatConfig& config)
    : config_{config} {}

Decision PolicyEngine::Evaluate(const Event& event) {
    Decision decision;

    if (!config_.policyEnabled) {
        decision.reason = "policy disabled";
        return decision;
    }

    decision.riskScore = CalculateRiskScore(event);

    if (decision.riskScore >= config_.policyBlockThreshold) {
        decision.type = DecisionType::Block;
        decision.reason = "risk exceeded block threshold";
    } else if (decision.riskScore >= config_.policyThrottleThreshold) {
        decision.type = DecisionType::Throttle;
        decision.reason = "risk exceeded throttle threshold";
    } else if (decision.riskScore >= config_.policySoftWarnThreshold) {
        decision.type = DecisionType::SoftWarn;
        decision.reason = "risk exceeded soft-warn threshold";
    } else {
        decision.type = DecisionType::Allow;
        decision.reason = "risk below thresholds";
    }

    return decision;
}

int PolicyEngine::CalculateRiskScore(const Event& event) {
    int score = 0;

    switch (event.action) {
    case ActionType::Login:
        score += 5;
        break;
    case ActionType::RoomJoin:
        score += 10;
        break;
    case ActionType::MessageSend:
        score += 15;
        break;
    case ActionType::Invite:
        score += 20;
        break;
    case ActionType::Ban:
        score += 25;
        break;
    }

    if (!event.actorExists) {
        score += 30;
    }

    if (!event.targetExists) {
        score += 20;
    }

    if (event.payloadSize > 500) {
        score += 20;
    } else if (event.payloadSize > 200) {
        score += 10;
    }

    if (event.target.empty()) {
        score += 10;
    }

    const auto now = Clock::now();
    const auto recentActionCount = GetRecentActionCount(event, now);
    if (recentActionCount > 20) {
        score += 35;
    } else if (recentActionCount > 10) {
        score += 20;
    } else if (recentActionCount > 5) {
        score += 10;
    }

    return score;
}

int PolicyEngine::GetRecentActionCount(const Event& event, const Clock::time_point& now) {
    auto& queue = recentActions_[BuildRateKey(event)];
    queue.push_back(now);

    while (!queue.empty()
        && std::chrono::duration_cast<std::chrono::seconds>(now - queue.front()).count() > RATE_WINDOW_SECONDS) {
        queue.pop_front();
    }

    return static_cast<int>(queue.size());
}

std::string PolicyEngine::BuildRateKey(const Event& event) const {
    std::ostringstream key;
    key << event.actorId << '|' << event.actorAddress << '|'
        << static_cast<int>(event.action);
    return key.str();
}

} // namespace policy
