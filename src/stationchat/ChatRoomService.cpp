#include "ChatRoomService.hpp"
#include "ChatAvatarService.hpp"
#include "Database.hpp"
#include "StreamUtils.hpp"
#include "StringUtils.hpp"

#include "easylogging++.h"

ChatRoomService::ChatRoomService(ChatAvatarService* avatarService, IDatabaseConnection* db)
    : avatarService_{avatarService}
    , db_{db} {}

ChatRoomService::~ChatRoomService() {}

void ChatRoomService::LoadRoomsFromStorage(const std::u16string& baseAddress) {
    rooms_.clear();

    
    char sql[] = "SELECT id, creator_id, creator_name, creator_address, room_name, room_topic, "
                 "room_password, room_prefix, room_address, room_attributes, room_max_size, "
                 "room_message_id, created_at, node_level FROM room WHERE room_address LIKE @baseAddress||'%'";

    StatementHandle stmt{db_->Prepare(sql)};

    int baseAddressIdx = stmt->BindParameterIndex("@baseAddress");

    auto baseAddressStr = FromWideString(baseAddress);
    LOG(INFO) << "Loading rooms for base address: " << baseAddressStr;
    stmt->BindText(baseAddressIdx, baseAddressStr);

    while (stmt->Step() == StatementStepResult::Row) {
        auto room = std::make_unique<ChatRoom>();
        std::string tmp;
        room->roomId_ = nextRoomId_++;
        room->dbId_ = stmt->ColumnInt(0);
        room->creatorId_ = stmt->ColumnInt(1);

        tmp = stmt->ColumnText(2);
        room->creatorName_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(3);
        room->creatorAddress_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(4);
        room->roomName_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(5);
        room->roomTopic_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(6);
        room->roomPassword_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(7);
        room->roomPrefix_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(8);
        room->roomAddress_ = std::u16string{std::begin(tmp), std::end(tmp)};

        room->roomAttributes_ = stmt->ColumnInt(9);
        room->maxRoomSize_ = stmt->ColumnInt(10);
        room->roomMessageId_ = stmt->ColumnInt(11);
        room->createTime_ = stmt->ColumnInt(12);
        room->nodeLevel_ = stmt->ColumnInt(13);

        if (!RoomExists(room->GetRoomAddress())) {
            rooms_.emplace_back(std::move(room));
        }
    }

    LOG(INFO) << "Rooms currently loaded: " << rooms_.size();
}

ChatRoom* ChatRoomService::CreateRoom(const ChatAvatar* creator,
    const std::u16string& roomName, const std::u16string& roomTopic, const std::u16string& roomPassword,
    uint32_t roomAttributes, uint32_t maxRoomSize, const std::u16string& roomAddress,
    const std::u16string& srcAddress) {
    ChatRoom* roomPtr = nullptr;

    if (RoomExists(roomAddress + u"+" + roomName)) {
        throw ChatResultException(ChatResultCode::ROOM_ALREADYEXISTS, "ChatRoom already exists");
    }

    LOG(INFO) << "Creating room " << FromWideString(roomName) << "@" << FromWideString(roomAddress) << " with attributes "
              << roomAttributes;

    rooms_.emplace_back(std::make_unique<ChatRoom>(this, nextRoomId_++, creator, roomName,
        roomTopic, roomPassword, roomAttributes, maxRoomSize, roomAddress, srcAddress));
    roomPtr = rooms_.back().get();

    if (roomPtr->IsPersistent()) {
        PersistNewRoom(*roomPtr);
    }

    return roomPtr;
}

void ChatRoomService::DestroyRoom(ChatRoom* room) {
    if (room->IsPersistent()) {
        DeleteRoom(room);
    }

    rooms_.erase(std::remove_if(std::begin(rooms_), std::end(rooms_),
        [room](const auto& trackedRoom) { return trackedRoom->GetRoomId() == room->GetRoomId(); }));
}

ChatResultCode ChatRoomService::PersistNewRoom(ChatRoom& room) {
    ChatResultCode result = ChatResultCode::SUCCESS;
    
    char sql[] = "INSERT INTO room (creator_id, creator_name, creator_address, room_name, "
                 "room_topic, room_password, room_prefix, room_address, room_attributes, "
                 "room_max_size, room_message_id, created_at, node_level) VALUES (@creator_id, "
                 "@creator_name, @creator_address, @room_name, @room_topic, @room_password, "
                 "@room_prefix, @room_address, @room_attributes, @room_max_size, @room_message_id, "
                 "@created_at, @node_level)";

    try {
        StatementHandle stmt{db_->Prepare(sql)};
        int creatorIdIdx = stmt->BindParameterIndex("@creator_id");
        int creatorNameIdx = stmt->BindParameterIndex("@creator_name");
        int creatorAddressIdx = stmt->BindParameterIndex("@creator_address");
        int roomNameIdx = stmt->BindParameterIndex("@room_name");
        int roomTopicIdx = stmt->BindParameterIndex("@room_topic");
        int roomPasswordIdx = stmt->BindParameterIndex("@room_password");
        int roomPrefixIdx = stmt->BindParameterIndex("@room_prefix");
        int roomAddressIdx = stmt->BindParameterIndex("@room_address");
        int roomAttributesIdx = stmt->BindParameterIndex("@room_attributes");
        int roomMaxSizeIdx = stmt->BindParameterIndex("@room_max_size");
        int roomMessageIdIdx = stmt->BindParameterIndex("@room_message_id");
        int createdAtIdx = stmt->BindParameterIndex("@created_at");
        int nodeLevelIdx = stmt->BindParameterIndex("@node_level");

        stmt->BindInt(creatorIdIdx, room.creatorId_);

        auto creatorName = FromWideString(room.creatorName_);
        stmt->BindText(creatorNameIdx, creatorName);

        auto creatorAddress = FromWideString(room.creatorAddress_);
        stmt->BindText(creatorAddressIdx, creatorAddress);

        auto roomName = FromWideString(room.roomName_);
        stmt->BindText(roomNameIdx, roomName);

        auto roomTopic = FromWideString(room.roomTopic_);
        stmt->BindText(roomTopicIdx, roomTopic);

        auto roomPassword = FromWideString(room.roomPassword_);
        stmt->BindText(roomPasswordIdx, roomPassword);

        auto roomPrefix = FromWideString(room.roomPrefix_);
        stmt->BindText(roomPrefixIdx, roomPrefix);

        auto roomAddress = FromWideString(room.roomAddress_);
        stmt->BindText(roomAddressIdx, roomAddress);

        stmt->BindInt(roomAttributesIdx, room.roomAttributes_);
        stmt->BindInt(roomMaxSizeIdx, room.maxRoomSize_);
        stmt->BindInt(roomMessageIdIdx, room.roomMessageId_);
        stmt->BindInt(createdAtIdx, room.createTime_);
        stmt->BindInt(nodeLevelIdx, room.nodeLevel_);

        if (stmt->Step() != StatementStepResult::Done) {
            result = ChatResultCode::DBFAIL;
        } else {
            room.dbId_ = static_cast<uint32_t>(db_->GetLastInsertId());
        }
    } catch (const DatabaseException&) {
        result = ChatResultCode::DBFAIL;
    }

    return result;
}

std::vector<ChatRoom*> ChatRoomService::GetRoomSummaries(
    const std::u16string& startNode, const std::u16string& filter) {
    std::vector<ChatRoom*> rooms;

    for (auto& room : rooms_) {
        auto& roomAddress = room->GetRoomAddress();
        if (roomAddress.compare(0, startNode.length(), startNode) == 0) {
            if (!room->IsPrivate()) {
                rooms.push_back(room.get());
            }
        }
    }

    return rooms;
}

bool ChatRoomService::RoomExists(const std::u16string& roomAddress) const {
    return std::find_if(std::begin(rooms_), std::end(rooms_), [roomAddress](auto& room) {
        return roomAddress.compare(room->GetRoomAddress()) == 0;
    }) != std::end(rooms_);
}

ChatRoom* ChatRoomService::GetRoom(const std::u16string& roomAddress) {
    ChatRoom* room = nullptr;

    auto find_iter = std::find_if(std::begin(rooms_), std::end(rooms_),
        [roomAddress](auto& room) { return roomAddress.compare(room->GetRoomAddress()) == 0; });

    if (find_iter != std::end(rooms_)) {
        room = find_iter->get();
    }

    return room;
}

std::vector<ChatRoom*> ChatRoomService::GetJoinedRooms(const ChatAvatar * avatar) {
    std::vector<ChatRoom*> rooms;

    for (auto& room : rooms_) {
        if (room->IsInRoom(avatar->GetAvatarId())) {
            rooms.push_back(room.get());
        }
    }

    return rooms;
}

void ChatRoomService::DeleteRoom(ChatRoom* room) {
        char sql[] = "DELETE FROM room WHERE id = @id";

    StatementHandle stmt{db_->Prepare(sql)};

    int idIdx = stmt->BindParameterIndex("@id");
    stmt->BindInt(idIdx, room->dbId_);

    stmt.ExpectDone();
}

void ChatRoomService::LoadModerators(ChatRoom * room) {
        char sql[] = "SELECT moderator_avatar_id FROM room_moderator WHERE room_id = @room_id";

    StatementHandle stmt{db_->Prepare(sql)};

    int roomIdIdx = stmt->BindParameterIndex("@room_id");
    stmt->BindInt(roomIdIdx, room->GetRoomId());

    while (stmt->Step() == StatementStepResult::Row) {
        uint32_t moderatorId = stmt->ColumnInt(0);
        room->moderators_.push_back(avatarService_->GetAvatar(moderatorId));
    }
}

void ChatRoomService::PersistModerator(uint32_t moderatorId, uint32_t roomId) {
        char sql[] = "INSERT IGNORE INTO room_moderator (moderator_avatar_id, room_id) VALUES (@moderator_avatar_id, @room_id)";

    StatementHandle stmt{db_->Prepare(sql)};

    int moderatorAvatarIdIdx = stmt->BindParameterIndex("@moderator_avatar_id");
    int roomIdIdx = stmt->BindParameterIndex("@room_id");

    stmt->BindInt(moderatorAvatarIdIdx, moderatorId);
    stmt->BindInt(roomIdIdx, roomId);

    stmt.ExpectDone();
}

void ChatRoomService::DeleteModerator(uint32_t moderatorId, uint32_t roomId) {
        char sql[] = "DELETE FROM room_moderator WHERE moderator_avatar_id = @moderator_avatar_id AND room_id = @room_id";

    StatementHandle stmt{db_->Prepare(sql)};

    int moderatorAvatarIdIdx = stmt->BindParameterIndex("@moderator_avatar_id");
    int roomIdIdx = stmt->BindParameterIndex("@room_id");

    stmt->BindInt(moderatorAvatarIdIdx, moderatorId);
    stmt->BindInt(roomIdIdx, roomId);

    stmt.ExpectDone();
}

void ChatRoomService::LoadAdministrators(ChatRoom * room) {
        char sql[] = "SELECT administrator_avatar_id FROM room_administrator WHERE room_id = @room_id";

    StatementHandle stmt{db_->Prepare(sql)};

    int roomIdIdx = stmt->BindParameterIndex("@room_id");
    stmt->BindInt(roomIdIdx, room->GetRoomId());

    while (stmt->Step() == StatementStepResult::Row) {
        uint32_t administratorId = stmt->ColumnInt(0);
        room->administrators_.push_back(avatarService_->GetAvatar(administratorId));
    }
}

void ChatRoomService::PersistAdministrator(uint32_t administratorId, uint32_t roomId) {
        char sql[] = "INSERT IGNORE INTO room_administrator (administrator_avatar_id, room_id) VALUES (@administrator_avatar_id, @room_id)";

    StatementHandle stmt{db_->Prepare(sql)};

    int administratorAvatarIdIdx = stmt->BindParameterIndex("@administrator_avatar_id");
    int roomIdIdx = stmt->BindParameterIndex("@room_id");

    stmt->BindInt(administratorAvatarIdIdx, administratorId);
    stmt->BindInt(roomIdIdx, roomId);

    stmt.ExpectDone();
}

void ChatRoomService::DeleteAdministrator(uint32_t administratorId, uint32_t roomId) {
        char sql[] = "DELETE FROM room_administrator WHERE administrator_avatar_id = @administrator_avatar_id AND room_id = @room_id";

    StatementHandle stmt{db_->Prepare(sql)};

    int administratorAvatarIdIdx = stmt->BindParameterIndex("@administrator_avatar_id");
    int roomIdIdx = stmt->BindParameterIndex("@room_id");

    stmt->BindInt(administratorAvatarIdIdx, administratorId);
    stmt->BindInt(roomIdIdx, roomId);

    stmt.ExpectDone();
}

void ChatRoomService::LoadBanned(ChatRoom * room) {
        char sql[] = "SELECT banned_avatar_id FROM room_ban WHERE room_id = @room_id";

    StatementHandle stmt{db_->Prepare(sql)};

    int roomIdIdx = stmt->BindParameterIndex("@room_id");
    stmt->BindInt(roomIdIdx, room->GetRoomId());

    while (stmt->Step() == StatementStepResult::Row) {
        uint32_t bannedId = stmt->ColumnInt(0);
        room->banned_.push_back(avatarService_->GetAvatar(bannedId));
    }
}

void ChatRoomService::PersistBanned(uint32_t bannedId, uint32_t roomId) {
        char sql[] = "INSERT IGNORE INTO room_ban (banned_avatar_id, room_id) VALUES (@banned_avatar_id, @room_id)";

    StatementHandle stmt{db_->Prepare(sql)};

    int bannedAvatarIdIdx = stmt->BindParameterIndex("@banned_avatar_id");
    int roomIdIdx = stmt->BindParameterIndex("@room_id");

    stmt->BindInt(bannedAvatarIdIdx, bannedId);
    stmt->BindInt(roomIdIdx, roomId);

    stmt.ExpectDone();
}

void ChatRoomService::DeleteBanned(uint32_t bannedId, uint32_t roomId) {
        char sql[] = "DELETE FROM room_ban WHERE banned_avatar_id = @banned_avatar_id AND room_id = @room_id";

    StatementHandle stmt{db_->Prepare(sql)};

    int bannedAvatarIdIdx = stmt->BindParameterIndex("@banned_avatar_id");
    int roomIdIdx = stmt->BindParameterIndex("@room_id");

    stmt->BindInt(bannedAvatarIdIdx, bannedId);
    stmt->BindInt(roomIdIdx, roomId);

    stmt.ExpectDone();
}
