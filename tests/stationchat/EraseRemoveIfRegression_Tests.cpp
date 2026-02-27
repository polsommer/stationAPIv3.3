#include "catch.hpp"

#define private public
#include "ChatAvatar.hpp"
#include "ChatAvatarService.hpp"
#include "ChatRoom.hpp"
#include "ChatRoomService.hpp"
#undef private

#include "Database.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

class NoopStatement final : public IStatement {
public:
    int BindParameterIndex(const std::string&) const override { return 1; }
    void BindInt(int, int64_t) override {}
    void BindText(int, const std::string&) override {}
    void BindBlob(int, const uint8_t*, size_t) override {}

    StatementStepResult Step() override { return StatementStepResult::Done; }

    int ColumnInt(int) const override { return 0; }
    std::string ColumnText(int) const override { return ""; }
    const uint8_t* ColumnBlob(int) const override { return nullptr; }
    int ColumnBytes(int) const override { return 0; }
};

class NoopTransaction final : public ITransaction {
public:
    void Commit() override {}
    void Rollback() override {}
};

class FakeDatabaseConnection final : public IDatabaseConnection {
public:
    std::unique_ptr<IStatement> Prepare(const std::string& sql) override {
        if (sql.find("INSERT INTO avatar") != std::string::npos) {
            ++lastInsertId_;
        }

        return std::make_unique<NoopStatement>();
    }

    std::unique_ptr<ITransaction> BeginTransaction() override {
        return std::make_unique<NoopTransaction>();
    }

    uint64_t GetLastInsertId() const override { return lastInsertId_; }
    std::string BackendName() const override { return "mariadb"; }
    const DatabaseCapabilities& Capabilities() const override { return capabilities_; }

private:
    uint64_t lastInsertId_ = 0;
    DatabaseCapabilities capabilities_{UpsertStrategy::InsertIgnore, BlobSemantics::NativeBlob,
        TransactionIsolationSupport::SerializableOnly};
};

ChatAvatar* MakeAvatar(ChatAvatarService& service, const std::u16string& name, uint32_t userId) {
    return service.CreateAvatar(name, u"corellia", userId, 0, u"bestine");
}



class PersistTrackingStatement final : public IStatement {
public:
    explicit PersistTrackingStatement(const std::string& sql,
        std::set<std::pair<uint32_t, uint32_t>>& persistedAdmins,
        std::set<std::pair<uint32_t, uint32_t>>& persistedMods,
        std::set<std::pair<uint32_t, uint32_t>>& persistedBans)
        : sql_{sql}
        , persistedAdmins_{persistedAdmins}
        , persistedMods_{persistedMods}
        , persistedBans_{persistedBans} {}

    int BindParameterIndex(const std::string& name) const override {
        auto it = parameterIndexes_.find(name);
        if (it != parameterIndexes_.end()) {
            return it->second;
        }

        int next = static_cast<int>(parameterIndexes_.size()) + 1;
        parameterIndexes_[name] = next;
        return next;
    }

    void BindInt(int index, int64_t value) override { boundInts_[index] = value; }
    void BindText(int, const std::string&) override {}
    void BindBlob(int, const uint8_t*, size_t) override {}

    StatementStepResult Step() override {
        if (executed_) {
            return StatementStepResult::Done;
        }

        executed_ = true;

        if (sql_.find("room_administrator") != std::string::npos
            && sql_.find("INSERT") != std::string::npos) {
            auto administratorId = static_cast<uint32_t>(GetBoundInt("@administrator_avatar_id"));
            auto roomId = static_cast<uint32_t>(GetBoundInt("@room_id"));
            persistedAdmins_.emplace(administratorId, roomId);
        }

        if (sql_.find("room_moderator") != std::string::npos
            && sql_.find("INSERT") != std::string::npos) {
            auto moderatorId = static_cast<uint32_t>(GetBoundInt("@moderator_avatar_id"));
            auto roomId = static_cast<uint32_t>(GetBoundInt("@room_id"));
            persistedMods_.emplace(moderatorId, roomId);
        }

        if (sql_.find("room_ban") != std::string::npos
            && sql_.find("INSERT") != std::string::npos) {
            auto bannedId = static_cast<uint32_t>(GetBoundInt("@banned_avatar_id"));
            auto roomId = static_cast<uint32_t>(GetBoundInt("@room_id"));
            persistedBans_.emplace(bannedId, roomId);
        }

        return StatementStepResult::Done;
    }

    int ColumnInt(int) const override { return 0; }
    std::string ColumnText(int) const override { return ""; }
    const uint8_t* ColumnBlob(int) const override { return nullptr; }
    int ColumnBytes(int) const override { return 0; }

private:
    int64_t GetBoundInt(const std::string& parameterName) const {
        auto parameterIndex = parameterIndexes_.at(parameterName);
        return boundInts_.at(parameterIndex);
    }

    std::string sql_;
    std::set<std::pair<uint32_t, uint32_t>>& persistedAdmins_;
    std::set<std::pair<uint32_t, uint32_t>>& persistedMods_;
    std::set<std::pair<uint32_t, uint32_t>>& persistedBans_;
    mutable std::map<std::string, int> parameterIndexes_;
    std::map<int, int64_t> boundInts_;
    bool executed_ = false;
};

class PersistTrackingDatabaseConnection final : public IDatabaseConnection {
public:
    std::unique_ptr<IStatement> Prepare(const std::string& sql) override {
        if (sql.find("INSERT INTO avatar") != std::string::npos) {
            ++lastInsertId_;
            return std::make_unique<NoopStatement>();
        }

        return std::make_unique<PersistTrackingStatement>(
            sql, persistedAdmins_, persistedModerators_, persistedBans_);
    }

    std::unique_ptr<ITransaction> BeginTransaction() override {
        return std::make_unique<NoopTransaction>();
    }

    uint64_t GetLastInsertId() const override { return lastInsertId_; }
    std::string BackendName() const override { return "mariadb"; }
    const DatabaseCapabilities& Capabilities() const override { return capabilities_; }

    bool HasPersistedAdministrator(uint32_t administratorId, uint32_t roomId) const {
        return persistedAdmins_.find(std::make_pair(administratorId, roomId)) != persistedAdmins_.end();
    }

    bool HasPersistedModerator(uint32_t moderatorId, uint32_t roomId) const {
        return persistedModerators_.find(std::make_pair(moderatorId, roomId)) != persistedModerators_.end();
    }

    bool HasPersistedBan(uint32_t bannedId, uint32_t roomId) const {
        return persistedBans_.find(std::make_pair(bannedId, roomId)) != persistedBans_.end();
    }

private:
    uint64_t lastInsertId_ = 0;
    DatabaseCapabilities capabilities_{UpsertStrategy::InsertIgnore, BlobSemantics::NativeBlob,
        TransactionIsolationSupport::SerializableOnly};
    std::set<std::pair<uint32_t, uint32_t>> persistedAdmins_;
    std::set<std::pair<uint32_t, uint32_t>> persistedModerators_;
    std::set<std::pair<uint32_t, uint32_t>> persistedBans_;
};

} // namespace

SCENARIO("chat avatar removals erase the entire removed tail range", "[stationchat][avatar]") {
    FakeDatabaseConnection db;
    ChatAvatarService service{&db};

    auto* owner = MakeAvatar(service, u"owner", 1);
    auto* keep = MakeAvatar(service, u"keep", 2);
    auto* remove = MakeAvatar(service, u"remove", 3);

    owner->friendList_ = {
        FriendContact{remove, u"a"},
        FriendContact{keep, u"b"},
        FriendContact{remove, u"c"}};

    owner->RemoveFriend(remove);

    REQUIRE(owner->friendList_.size() == 1);
    REQUIRE(owner->friendList_[0].frnd->GetAvatarId() == keep->GetAvatarId());

    owner->ignoreList_ = {IgnoreContact{remove}, IgnoreContact{keep}, IgnoreContact{remove}};

    owner->RemoveIgnore(remove);

    REQUIRE(owner->ignoreList_.size() == 1);
    REQUIRE(owner->ignoreList_[0].ignored->GetAvatarId() == keep->GetAvatarId());
}

SCENARIO("chat room and room service removals do not leave ghost entries", "[stationchat][room]") {
    FakeDatabaseConnection db;
    ChatAvatarService avatarService{&db};

    auto* creator = MakeAvatar(avatarService, u"creator", 10);
    auto* keep = MakeAvatar(avatarService, u"keep", 11);
    auto* remove = MakeAvatar(avatarService, u"remove", 12);

    ChatRoom room{nullptr, 500, creator, u"room", u"topic", u"", 0, 50, u"swg", u"swg"};

    room.avatars_ = {remove, keep, remove};
    room.LeaveRoom(remove);
    REQUIRE(room.avatars_.size() == 1);
    REQUIRE(room.avatars_[0]->GetAvatarId() == keep->GetAvatarId());

    room.administrators_ = {remove, keep, remove};
    room.RemoveAdministrator(creator->GetAvatarId(), remove->GetAvatarId());
    REQUIRE(room.administrators_.size() == 1);
    REQUIRE(room.administrators_[0]->GetAvatarId() == keep->GetAvatarId());

    room.moderators_ = {creator, remove, keep, remove};
    room.RemoveModerator(creator->GetAvatarId(), remove->GetAvatarId());
    REQUIRE(room.moderators_.size() == 2);
    REQUIRE(room.moderators_[0]->GetAvatarId() == creator->GetAvatarId());
    REQUIRE(room.moderators_[1]->GetAvatarId() == keep->GetAvatarId());

    room.banned_ = {remove, keep, remove};
    room.RemoveBanned(creator->GetAvatarId(), remove->GetAvatarId());
    REQUIRE(room.banned_.size() == 1);
    REQUIRE(room.banned_[0]->GetAvatarId() == keep->GetAvatarId());

    room.invited_ = {remove, keep, remove};
    room.RemoveInvite(creator->GetAvatarId(), remove->GetAvatarId());
    REQUIRE(room.invited_.size() == 1);
    REQUIRE(room.invited_[0]->GetAvatarId() == keep->GetAvatarId());

    ChatRoomService roomService{&avatarService, &db};
    auto trackedA = std::make_unique<ChatRoom>();
    trackedA->roomId_ = 100;
    auto trackedB = std::make_unique<ChatRoom>();
    trackedB->roomId_ = 200;
    auto trackedC = std::make_unique<ChatRoom>();
    trackedC->roomId_ = 200;

    auto* trackedBPtr = trackedB.get();

    roomService.rooms_.emplace_back(std::move(trackedA));
    roomService.rooms_.emplace_back(std::move(trackedB));
    roomService.rooms_.emplace_back(std::move(trackedC));

    roomService.DestroyRoom(trackedBPtr);

    REQUIRE(roomService.rooms_.size() == 1);
    REQUIRE(roomService.rooms_[0]->GetRoomId() == 100);
}

SCENARIO("logout removes all matching online avatars", "[stationchat][avatarservice]") {
    FakeDatabaseConnection db;
    ChatAvatarService service{&db};

    auto* keep = MakeAvatar(service, u"keep", 21);
    auto* remove = MakeAvatar(service, u"remove", 22);

    remove->isOnline_ = true;
    keep->isOnline_ = true;
    service.onlineAvatars_ = {remove, keep, remove};

    service.LogoutAvatar(remove);

    REQUIRE(service.onlineAvatars_.size() == 1);
    REQUIRE(service.onlineAvatars_[0]->GetAvatarId() == keep->GetAvatarId());
    REQUIRE(remove->IsOnline() == false);
}


SCENARIO("persistent room role adds persist into role tables", "[stationchat][room][roles]") {
    PersistTrackingDatabaseConnection db;
    ChatAvatarService avatarService{&db};
    ChatRoomService roomService{&avatarService, &db};

    auto* creator = MakeAvatar(avatarService, u"creator", 101);
    auto* administrator = MakeAvatar(avatarService, u"admin", 102);
    auto* moderator = MakeAvatar(avatarService, u"mod", 103);

    auto* persistentRoom = roomService.CreateRoom(creator, u"persist", u"topic", u"",
        static_cast<uint32_t>(RoomAttributes::PERSISTENT), 50, u"swg", u"swg");

    persistentRoom->AddAdministrator(creator->GetAvatarId(), administrator);
    persistentRoom->AddModerator(creator->GetAvatarId(), moderator);

    REQUIRE(db.HasPersistedAdministrator(administrator->GetAvatarId(), persistentRoom->GetRoomId()));
    REQUIRE(db.HasPersistedModerator(moderator->GetAvatarId(), persistentRoom->GetRoomId()));
    REQUIRE_FALSE(db.HasPersistedBan(administrator->GetAvatarId(), persistentRoom->GetRoomId()));
    REQUIRE_FALSE(db.HasPersistedBan(moderator->GetAvatarId(), persistentRoom->GetRoomId()));
}
