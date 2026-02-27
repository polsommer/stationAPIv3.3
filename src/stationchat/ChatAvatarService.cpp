#include "ChatAvatarService.hpp"
#include "ChatAvatar.hpp"
#include "Database.hpp"
#include "StringUtils.hpp"

#include <easylogging++.h>

ChatAvatarService::ChatAvatarService(IDatabaseConnection* db)
    : db_{db} {}

ChatAvatarService::~ChatAvatarService() {}

ChatAvatar* ChatAvatarService::GetAvatar(const std::u16string& name, const std::u16string& address) {
    ChatAvatar* avatar = GetCachedAvatar(name, address);

    if (!avatar) {
        auto loadedAvatar = LoadStoredAvatar(name, address);
        if (loadedAvatar != nullptr) {
            avatar = loadedAvatar.get();
            avatarCache_.emplace_back(std::move(loadedAvatar));

            LoadFriendList(avatar);
            LoadIgnoreList(avatar);
        }
    }

    return avatar;
}

ChatAvatar* ChatAvatarService::GetAvatar(uint32_t avatarId) {
    ChatAvatar* avatar = GetCachedAvatar(avatarId);

    if (!avatar) {
        auto loadedAvatar = LoadStoredAvatar(avatarId);
        if (loadedAvatar != nullptr) {
            avatar = loadedAvatar.get();
            avatarCache_.emplace_back(std::move(loadedAvatar));

            LoadFriendList(avatar);
            LoadIgnoreList(avatar);
        }
    }

    return avatar;
}

ChatAvatar* ChatAvatarService::CreateAvatar(const std::u16string& name, const std::u16string& address,
    uint32_t userId, uint32_t loginAttributes, const std::u16string& loginLocation) {
    auto tmp
        = std::make_unique<ChatAvatar>(this, name, address, userId, loginAttributes, loginLocation);
    auto avatar = tmp.get();

    InsertAvatar(avatar);

    avatarCache_.emplace_back(std::move(tmp));

    return avatar;
}

void ChatAvatarService::DestroyAvatar(ChatAvatar* avatar) {
    DeleteAvatar(avatar);
    LogoutAvatar(avatar);
    RemoveCachedAvatar(avatar->GetAvatarId());
}

void ChatAvatarService::LoginAvatar(ChatAvatar* avatar) {
    avatar->isOnline_ = true;

    if (!IsOnline(avatar)) {
        onlineAvatars_.push_back(avatar);
    }
}

void ChatAvatarService::LogoutAvatar(ChatAvatar* avatar) {
	if(!avatar->isOnline_) return;
    avatar->isOnline_ = false;

    onlineAvatars_.erase(std::remove_if(
        std::begin(onlineAvatars_), std::end(onlineAvatars_), [avatar](auto onlineAvatar) {
            return onlineAvatar->GetAvatarId() == avatar->GetAvatarId();
        }));
}

void ChatAvatarService::PersistAvatar(const ChatAvatar* avatar) { UpdateAvatar(avatar); }

void ChatAvatarService::PersistFriend(
    uint32_t srcAvatarId, uint32_t destAvatarId, const std::u16string& comment) {
        char sql[] = "INSERT INTO friend (avatar_id, friend_avatar_id, comment) VALUES (@avatar_id, "
                 "@friend_avatar_id, @comment)";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int friendAvatarIdIdx = stmt->BindParameterIndex("@friend_avatar_id");
    int commentIdx = stmt->BindParameterIndex("@comment");

    std::string commentStr = FromWideString(comment);

    stmt->BindInt(avatarIdIdx, srcAvatarId);
    stmt->BindInt(friendAvatarIdIdx, destAvatarId);
    stmt->BindText(commentIdx, commentStr);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

void ChatAvatarService::PersistIgnore(uint32_t srcAvatarId, uint32_t destAvatarId) {
        char sql[] = "INSERT INTO ignore (avatar_id, ignore_avatar_id) VALUES (@avatar_id, "
                 "@ignore_avatar_id)";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int ignoreAvatarIdIdx = stmt->BindParameterIndex("@ignore_avatar_id");

    stmt->BindInt(avatarIdIdx, srcAvatarId);
    stmt->BindInt(ignoreAvatarIdIdx, destAvatarId);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

void ChatAvatarService::RemoveFriend(uint32_t srcAvatarId, uint32_t destAvatarId) {
    
    char sql[] = "DELETE FROM friend WHERE avatar_id = @avatar_id AND friend_avatar_id = "
                 "@friend_avatar_id";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int friendAvatarIdIdx = stmt->BindParameterIndex("@friend_avatar_id");

    stmt->BindInt(avatarIdIdx, srcAvatarId);
    stmt->BindInt(friendAvatarIdIdx, destAvatarId);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

void ChatAvatarService::RemoveIgnore(uint32_t srcAvatarId, uint32_t destAvatarId) {
    
    char sql[] = "DELETE FROM ignore WHERE avatar_id = @avatar_id AND ignore_avatar_id = "
                 "@ignore_avatar_id";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int ignoreAvatarIdIdx = stmt->BindParameterIndex("@ignore_avatar_id");

    stmt->BindInt(avatarIdIdx, srcAvatarId);
    stmt->BindInt(ignoreAvatarIdIdx, destAvatarId);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

void ChatAvatarService::UpdateFriendComment(
    uint32_t srcAvatarId, uint32_t destAvatarId, const std::u16string& comment) {
        char sql[] = "UPDATE friend SET comment = @comment WHERE avatar_id = @avatar_id AND "
                 "friend_avatar_id = @friend_avatar_id";

    auto stmt = db_->Prepare(sql);

    int commentIdx = stmt->BindParameterIndex("@comment");
    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int friendAvatarIdIdx = stmt->BindParameterIndex("@friend_avatar_id");

    std::string commentStr = FromWideString(comment);

    stmt->BindText(commentIdx, commentStr);
    stmt->BindInt(avatarIdIdx, srcAvatarId);
    stmt->BindInt(friendAvatarIdIdx, destAvatarId);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

ChatAvatar* ChatAvatarService::GetCachedAvatar(
    const std::u16string& name, const std::u16string& address) {
    ChatAvatar* avatar = nullptr;

    // First look for the avatar in the cache
    auto find_iter = std::find_if(
        std::begin(avatarCache_), std::end(avatarCache_), [name, address](const auto& avatar) {
            return avatar->name_.compare(name) == 0 && avatar->address_.compare(address) == 0;
        });

    if (find_iter != std::end(avatarCache_)) {
        avatar = find_iter->get();
    }

    return avatar;
}

ChatAvatar* ChatAvatarService::GetCachedAvatar(uint32_t avatarId) {
    ChatAvatar* avatar = nullptr;

    // First look for the avatar in the cache
    auto find_iter = std::find_if(std::begin(avatarCache_), std::end(avatarCache_),
        [avatarId](const auto& avatar) { return avatar->avatarId_ == avatarId; });

    if (find_iter != std::end(avatarCache_)) {
        avatar = find_iter->get();
    }

    return avatar;
}

void ChatAvatarService::RemoveCachedAvatar(uint32_t avatarId) {
    auto remove_iter = std::remove_if(std::begin(avatarCache_), std::end(avatarCache_),
                                  [avatarId](const auto& avatar) { return avatar->avatarId_ == avatarId; });

    if (remove_iter != std::end(avatarCache_)) {
        avatarCache_.erase(remove_iter);
    }
}

void ChatAvatarService::RemoveAsFriendOrIgnoreFromAll(const ChatAvatar* avatar) {
    for (auto& cachedAvatar : avatarCache_) {
        if (cachedAvatar->IsFriend(avatar)) {
            cachedAvatar->RemoveFriend(avatar);
        }

        if (cachedAvatar->IsIgnored(avatar)) {
            cachedAvatar->RemoveIgnore(avatar);
        }
    }
}

std::unique_ptr<ChatAvatar> ChatAvatarService::LoadStoredAvatar(
    const std::u16string& name, const std::u16string& address) {
    std::unique_ptr<ChatAvatar> avatar{nullptr};

    
    char sql[] = "SELECT id, user_id, name, address, attributes FROM avatar WHERE name = @name AND "
                 "address = @address";

    auto stmt = db_->Prepare(sql);

    std::string nameStr = FromWideString(name);
    std::string addressStr = FromWideString(address);

    int nameIdx = stmt->BindParameterIndex("@name");
    int addressIdx = stmt->BindParameterIndex("@address");

    stmt->BindText(nameIdx, nameStr);
    stmt->BindText(addressIdx, addressStr);

    if (stmt->Step() == StatementStepResult::Row) {
        avatar = std::make_unique<ChatAvatar>(this);
        avatar->avatarId_ = stmt->ColumnInt(0);
        avatar->userId_ = stmt->ColumnInt(1);

        auto tmp = stmt->ColumnText(2);
        avatar->name_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(3);
        avatar->address_ = std::u16string(std::begin(tmp), std::end(tmp));

        avatar->attributes_ = stmt->ColumnInt(4);
    }

    
    return avatar;
}

std::unique_ptr<ChatAvatar> ChatAvatarService::LoadStoredAvatar(uint32_t avatarId) {
    std::unique_ptr<ChatAvatar> avatar{nullptr};

    
    char sql[] = "SELECT id, user_id, name, address, attributes FROM avatar WHERE id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(avatarIdIdx, avatarId);

    if (stmt->Step() == StatementStepResult::Row) {
        avatar = std::make_unique<ChatAvatar>(this);
        avatar->avatarId_ = stmt->ColumnInt(0);
        avatar->userId_ = stmt->ColumnInt(1);

        auto tmp = stmt->ColumnText(2);
        avatar->name_ = std::u16string{std::begin(tmp), std::end(tmp)};

        tmp = stmt->ColumnText(3);
        avatar->address_ = std::u16string(std::begin(tmp), std::end(tmp));

        avatar->attributes_ = stmt->ColumnInt(4);
    }

    
    return avatar;
}

void ChatAvatarService::InsertAvatar(ChatAvatar* avatar) {
    CHECK_NOTNULL(avatar);
    
    char sql[] = "INSERT INTO avatar (user_id, name, address, attributes) VALUES (@user_id, @name, "
                 "@address, @attributes)";

    auto stmt = db_->Prepare(sql);

    std::string nameStr = FromWideString(avatar->name_);
    std::string addressStr = FromWideString(avatar->address_);

    int userIdIdx = stmt->BindParameterIndex("@user_id");
    int nameIdx = stmt->BindParameterIndex("@name");
    int addressIdx = stmt->BindParameterIndex("@address");
    int attributesIdx = stmt->BindParameterIndex("@attributes");

    stmt->BindInt(userIdIdx, avatar->userId_);
    stmt->BindText(nameIdx, nameStr);
    stmt->BindText(addressIdx, addressStr);
    stmt->BindInt(attributesIdx, avatar->attributes_);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }

    avatar->avatarId_ = static_cast<uint32_t>(db_->GetLastInsertId());
}

void ChatAvatarService::UpdateAvatar(const ChatAvatar* avatar) {
    CHECK_NOTNULL(avatar);
    
    char sql[] = "UPDATE avatar SET user_id = @user_id, name = @name, address = @address, "
                 "attributes = @attributes "
                 "WHERE id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    std::string nameStr = FromWideString(avatar->name_);
    std::string addressStr = FromWideString(avatar->address_);

    int userIdIdx = stmt->BindParameterIndex("@user_id");
    int nameIdx = stmt->BindParameterIndex("@name");
    int addressIdx = stmt->BindParameterIndex("@address");
    int attributesIdx = stmt->BindParameterIndex("@attributes");
    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(userIdIdx, avatar->userId_);
    stmt->BindText(nameIdx, nameStr);
    stmt->BindText(addressIdx, addressStr);
    stmt->BindInt(attributesIdx, avatar->attributes_);
    stmt->BindInt(avatarIdIdx, avatar->avatarId_);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

void ChatAvatarService::DeleteAvatar(ChatAvatar* avatar) {
    CHECK_NOTNULL(avatar);
    
    char sql[] = "DELETE FROM avatar WHERE id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(avatarIdIdx, avatar->avatarId_);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}

void ChatAvatarService::LoadFriendList(ChatAvatar* avatar) {
    
    char sql[] = "SELECT friend_avatar_id, comment FROM friend WHERE avatar_id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(avatarIdIdx, avatar->avatarId_);

    uint32_t tmpFriendId;
    std::string tmpComment;
    while (stmt->Step() == StatementStepResult::Row) {
        tmpFriendId = stmt->ColumnInt(0);
        tmpComment = stmt->ColumnText(1);

        auto friendAvatar = GetAvatar(tmpFriendId);

        avatar->friendList_.emplace_back(friendAvatar, ToWideString(tmpComment));
    }
}

void ChatAvatarService::LoadIgnoreList(ChatAvatar* avatar) {
    
    char sql[] = "SELECT ignore_avatar_id FROM ignore WHERE avatar_id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(avatarIdIdx, avatar->avatarId_);

    uint32_t tmpIgnoreId;
    while (stmt->Step() == StatementStepResult::Row) {
        tmpIgnoreId = stmt->ColumnInt(0);

        auto ignoreAvatar = GetAvatar(tmpIgnoreId);

        avatar->ignoreList_.emplace_back(ignoreAvatar);
    }
}

bool ChatAvatarService::IsOnline(const ChatAvatar * avatar) const {
    for (auto onlineAvatar : onlineAvatars_) {
        if (onlineAvatar->GetAvatarId() == avatar->GetAvatarId()) {
            return true;
        }
    }

    return false;
}
