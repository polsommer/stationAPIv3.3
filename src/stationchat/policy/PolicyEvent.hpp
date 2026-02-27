#pragma once

#include <cstdint>
#include <string>

namespace policy {

enum class ActionType {
    Login,
    RoomJoin,
    MessageSend,
    Invite,
    Ban,
};

struct Event {
    ActionType action;
    uint32_t actorId = 0;
    std::string actorAddress;
    std::string target;
    std::size_t payloadSize = 0;
    bool actorExists = true;
    bool targetExists = true;
};

} // namespace policy
