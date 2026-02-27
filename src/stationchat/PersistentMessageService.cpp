#include "PersistentMessageService.hpp"

#include "Database.hpp"
#include "StringUtils.hpp"


PersistentMessageService::PersistentMessageService(IDatabaseConnection* db)
    : db_{db} {}

PersistentMessageService::~PersistentMessageService() {}

void PersistentMessageService::StoreMessage(PersistentMessage& message) {
    
    char sql[] = "INSERT INTO persistent_message (avatar_id, from_name, from_address, subject, "
                 "sent_time, status, "
                 "folder, category, message, oob) VALUES (@avatar_id, @from_name, @from_address, "
                 "@subject, @sent_time, @status, @folder, @category, @message, @oob)";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int fromNameIdx = stmt->BindParameterIndex("@from_name");
    int fromAddressIdx = stmt->BindParameterIndex("@from_address");
    int subjectIdx = stmt->BindParameterIndex("@subject");
    int sentTimeIdx = stmt->BindParameterIndex("@sent_time");
    int statusIdx = stmt->BindParameterIndex("@status");
    int folderIdx = stmt->BindParameterIndex("@folder");
    int categoryIdx = stmt->BindParameterIndex("@category");
    int messageIdx = stmt->BindParameterIndex("@message");
    int oobIdx = stmt->BindParameterIndex("@oob");

    stmt->BindInt(avatarIdIdx, message.header.avatarId);

    std::string fromName = FromWideString(message.header.fromName);
    stmt->BindText(fromNameIdx, fromName);

    std::string fromAddress = FromWideString(message.header.fromAddress);
    stmt->BindText(fromAddressIdx, fromAddress);

    std::string subject = FromWideString(message.header.subject);
    stmt->BindText(subjectIdx, subject);

    stmt->BindInt(sentTimeIdx, message.header.sentTime);
    stmt->BindInt(statusIdx, static_cast<uint32_t>(message.header.status));

    std::string folder = FromWideString(message.header.folder);
    stmt->BindText(folderIdx, folder);

    std::string category = FromWideString(message.header.category);
    stmt->BindText(categoryIdx, category);

    std::string msg = FromWideString(message.message);
    stmt->BindText(messageIdx, msg);

    stmt->BindBlob(oobIdx, reinterpret_cast<const uint8_t*>(message.oob.data()),
        message.oob.size() * sizeof(uint16_t));

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
}

    message.header.messageId = static_cast<uint32_t>(db_->GetLastInsertId());
}

std::vector<PersistentHeader> PersistentMessageService::GetMessageHeaders(uint32_t avatarId) {
    std::vector<PersistentHeader> headers;
    
    char sql[] = "SELECT id, avatar_id, from_name, from_address, subject, sent_time, status, "
                 "folder, category, message, oob FROM persistent_message WHERE avatar_id = "
                 "@avatar_id AND status IN (1, 2, 3)";

    auto stmt = db_->Prepare(sql);

    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(avatarIdIdx, avatarId);

    while (stmt->Step() == StatementStepResult::Row) {
        PersistentHeader header;

        header.messageId = stmt->ColumnInt(0);
        header.avatarId = stmt->ColumnInt(1);

        auto tmp = stmt->ColumnText(2);
        header.fromName = std::u16string(std::begin(tmp), std::end(tmp));

        tmp = stmt->ColumnText(3);
        header.fromAddress = std::u16string(std::begin(tmp), std::end(tmp));

        tmp = stmt->ColumnText(4);
        header.subject = std::u16string(std::begin(tmp), std::end(tmp));

        header.sentTime = stmt->ColumnInt(5);
        header.status = static_cast<PersistentState>(stmt->ColumnInt(6));

        tmp = stmt->ColumnText(7);
        header.folder = std::u16string(std::begin(tmp), std::end(tmp));

        tmp = stmt->ColumnText(8);
        header.category = std::u16string(std::begin(tmp), std::end(tmp));

        headers.push_back(std::move(header));
    }

    return headers;
}

PersistentMessage PersistentMessageService::GetPersistentMessage(
    uint32_t avatarId, uint32_t messageId) {
    
    char sql[] = "SELECT id, avatar_id, from_name, from_address, subject, sent_time, status, "
                 "folder, category, message, oob FROM persistent_message WHERE id = @message_id "
                 "AND avatar_id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    int messageIdIdx = stmt->BindParameterIndex("@message_id");
    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(messageIdIdx, messageId);
    stmt->BindInt(avatarIdIdx, avatarId);

    if (stmt->Step() != StatementStepResult::Row) {
        throw ChatResultException{ChatResultCode::PMSGNOTFOUND};
    }

    std::string tmp;

    PersistentMessage message;
    message.header.messageId = messageId;
    message.header.avatarId = avatarId;

    tmp = stmt->ColumnText(2);
    message.header.fromName = std::u16string(std::begin(tmp), std::end(tmp));

    tmp = stmt->ColumnText(3);
    message.header.fromAddress = std::u16string(std::begin(tmp), std::end(tmp));

    tmp = stmt->ColumnText(4);
    message.header.subject = std::u16string(std::begin(tmp), std::end(tmp));

    message.header.sentTime = stmt->ColumnInt(5);
    message.header.status = static_cast<PersistentState>(stmt->ColumnInt(6));

    tmp = stmt->ColumnText(7);
    message.header.folder = std::u16string(std::begin(tmp), std::end(tmp));

    tmp = stmt->ColumnText(8);
    message.header.category = std::u16string(std::begin(tmp), std::end(tmp));

    tmp = stmt->ColumnText(9);
    message.message = std::u16string(std::begin(tmp), std::end(tmp));

    int size = stmt->ColumnBytes(10);
    const uint8_t* data = stmt->ColumnBlob(10);

    if (size % static_cast<int>(sizeof(uint16_t)) != 0) {
        throw DatabaseException{"persistent_message.oob blob has invalid UTF-16 byte length"};
    }

    const int codeUnitCount = size / static_cast<int>(sizeof(uint16_t));
    message.oob.resize(codeUnitCount);
    for (int i = 0; i < codeUnitCount; ++i) {
        uint16_t value = 0;
        value = static_cast<uint16_t>(data[i * 2])
            | static_cast<uint16_t>(static_cast<uint16_t>(data[i * 2 + 1]) << 8);
        message.oob[i] = value;
    }

    
    if (message.header.status == PersistentState::NEW) {
        UpdateMessageStatus(
            message.header.avatarId, message.header.messageId, PersistentState::READ);
    }

    return message;
}

void PersistentMessageService::UpdateMessageStatus(
    uint32_t avatarId, uint32_t messageId, PersistentState status) {
    
    char sql[] = "UPDATE persistent_message SET status = @status WHERE id = @message_id AND "
                 "avatar_id = @avatar_id";

    auto stmt = db_->Prepare(sql);

    int statusIdx = stmt->BindParameterIndex("@status");
    int messageIdIdx = stmt->BindParameterIndex("@message_id");
    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");

    stmt->BindInt(statusIdx, static_cast<uint32_t>(status));
    stmt->BindInt(messageIdIdx, messageId);
    stmt->BindInt(avatarIdIdx, avatarId);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }

}

void PersistentMessageService::BulkUpdateMessageStatus(
    uint32_t avatarId, const std::u16string& category, PersistentState newStatus)
{
    
    char sql[] = "UPDATE persistent_message SET status = @status WHERE avatar_id = @avatar_id AND "
             "category = @category";

    auto stmt = db_->Prepare(sql);

    int statusIdx = stmt->BindParameterIndex("@status");
    int avatarIdIdx = stmt->BindParameterIndex("@avatar_id");
    int categoryIdx = stmt->BindParameterIndex("@category");

    stmt->BindInt(statusIdx, static_cast<uint32_t>(newStatus));
    stmt->BindInt(avatarIdIdx, avatarId);
    std::string cat = FromWideString(category);
    stmt->BindText(categoryIdx, cat);

    if (stmt->Step() != StatementStepResult::Done) {
        throw DatabaseException{"expected statement done"};
    }
}
