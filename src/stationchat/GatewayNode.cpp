#include "GatewayNode.hpp"

#include "ChatAvatarService.hpp"
#include "ChatRoomService.hpp"
#include "DatabaseFactory.hpp"
#include "PersistentMessageService.hpp"
#include "StationChatConfig.hpp"

GatewayNode::GatewayNode(StationChatConfig& config)
    : Node(this, config.gatewayAddress, config.gatewayPort, config.bindToIp)
    , config_{config}
    , db_{CreateDatabaseConnection(config)} {
    avatarService_ = std::make_unique<ChatAvatarService>(db_.get());
    roomService_ = std::make_unique<ChatRoomService>(avatarService_.get(), db_.get());
    messageService_ = std::make_unique<PersistentMessageService>(db_.get());
}

GatewayNode::~GatewayNode() = default;

ChatAvatarService* GatewayNode::GetAvatarService() { return avatarService_.get(); }

ChatRoomService* GatewayNode::GetRoomService() { return roomService_.get(); }

PersistentMessageService* GatewayNode::GetMessageService() {
    return messageService_.get();
}

StationChatConfig& GatewayNode::GetConfig() { return config_; }

void GatewayNode::RegisterClientAddress(const std::u16string & address, GatewayClient * client) {
    clientAddressMap_[address] = client;
}

void GatewayNode::OnTick() {}
