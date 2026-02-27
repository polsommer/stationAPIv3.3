#include "DatabaseMariaDb.hpp"

#include "SqlParameterAdapter.hpp"

#include <mysql/mysql.h>

#include <array>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string EscapeString(MYSQL* handle, const std::string& input) {
    std::string out;
    out.resize(input.size() * 2 + 1);
    auto written = mysql_real_escape_string(handle, out.data(), input.c_str(), input.size());
    out.resize(written);
    return out;
}

std::string BlobToHex(const uint8_t* data, size_t length) {
    static const std::array<char, 16> kHex = {'0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

    std::string out;
    out.reserve(length * 2 + 3);
    out += "X'";
    for (size_t i = 0; i < length; ++i) {
        uint8_t value = data[i];
        out.push_back(kHex[(value >> 4) & 0x0F]);
        out.push_back(kHex[value & 0x0F]);
    }
    out.push_back('\'');
    return out;
}

class MariaDbStatement final : public IStatement {
public:
    MariaDbStatement(MYSQL* handle, const std::string& sql)
        : handle_{handle}
        , executed_{false}
        , rowIndex_{0} {
        normalizedSql_ = NormalizeNamedParameters(sql);
        boundValues_.resize(normalizedSql_.positionsByLogicalIndex.size());
    }

    int BindParameterIndex(const std::string& name) const override {
        auto iter = normalizedSql_.logicalIndexByName.find(name);
        if (iter == normalizedSql_.logicalIndexByName.end()) {
            throw DatabaseException("mariadb missing parameter: " + name);
        }
        return iter->second;
    }

    void BindInt(int index, int64_t value) override {
        EnsureIndex(index);
        auto& slot = boundValues_[index];
        slot.type = BoundValue::Type::Int;
        slot.intValue = value;
    }

    void BindText(int index, const std::string& value) override {
        EnsureIndex(index);
        auto& slot = boundValues_[index];
        slot.type = BoundValue::Type::Text;
        slot.textValue = value;
    }

    void BindBlob(int index, const uint8_t* data, size_t length) override {
        EnsureIndex(index);
        auto& slot = boundValues_[index];
        slot.type = BoundValue::Type::Blob;
        slot.blobValue.assign(data, data + length);
    }

    StatementStepResult Step() override {
        if (!executed_) {
            Execute();
        }

        if (result_) {
            if (rowIndex_ < rowCount_) {
                currentRow_ = mysql_fetch_row(result_);
                currentLengths_ = mysql_fetch_lengths(result_);
                ++rowIndex_;
                return StatementStepResult::Row;
            }
            return StatementStepResult::Done;
        }

        return StatementStepResult::Done;
    }

    int ColumnInt(int index) const override {
        return std::stoi(ColumnText(index));
    }

    std::string ColumnText(int index) const override {
        if (!currentRow_) {
            return "";
        }
        const char* value = currentRow_[index];
        if (!value) {
            return "";
        }
        return std::string(value, currentLengths_[index]);
    }

    const uint8_t* ColumnBlob(int index) const override {
        if (!currentRow_) {
            return nullptr;
        }
        return reinterpret_cast<const uint8_t*>(currentRow_[index]);
    }

    int ColumnBytes(int index) const override {
        if (!currentLengths_) {
            return 0;
        }
        return static_cast<int>(currentLengths_[index]);
    }

    ~MariaDbStatement() override {
        if (result_) {
            mysql_free_result(result_);
        }
    }

private:
    struct BoundValue {
        enum class Type {
            None,
            Int,
            Text,
            Blob
        };

        Type type = Type::None;
        int64_t intValue = 0;
        std::string textValue;
        std::vector<uint8_t> blobValue;
    };

    void EnsureIndex(int index) {
        if (index < 0 || static_cast<size_t>(index) >= boundValues_.size()) {
            throw DatabaseException("mariadb invalid bind index");
        }
    }

    std::string RenderValue(const BoundValue& value) const {
        switch (value.type) {
        case BoundValue::Type::Int:
            return std::to_string(value.intValue);
        case BoundValue::Type::Text:
            return "'" + EscapeString(handle_, value.textValue) + "'";
        case BoundValue::Type::Blob:
            return BlobToHex(value.blobValue.data(), value.blobValue.size());
        case BoundValue::Type::None:
            return "NULL";
        }
        return "NULL";
    }

    void Execute() {
        std::vector<std::string> rendered;
        rendered.reserve(boundValues_.size());
        for (const auto& value : boundValues_) {
            rendered.push_back(RenderValue(value));
        }

        std::string sql;
        sql.reserve(normalizedSql_.sql.size() + 64);
        size_t parameterIndex = 0;
        for (char c : normalizedSql_.sql) {
            if (c == '?') {
                if (parameterIndex >= normalizedSql_.logicalIndexByPosition.size()) {
                    throw DatabaseException("mariadb missing parameter value");
                }
                int logicalIndex = normalizedSql_.logicalIndexByPosition[parameterIndex++];
                sql += rendered[static_cast<size_t>(logicalIndex)];
            } else {
                sql.push_back(c);
            }
        }

        if (mysql_real_query(handle_, sql.c_str(), sql.size()) != 0) {
            throw DatabaseException("mariadb query failed: " + std::string(mysql_error(handle_)));
        }

        result_ = mysql_store_result(handle_);
        if (!result_ && mysql_field_count(handle_) != 0) {
            throw DatabaseException("mariadb store result failed: " + std::string(mysql_error(handle_)));
        }

        if (result_) {
            rowCount_ = mysql_num_rows(result_);
        }

        executed_ = true;
    }

    MYSQL* handle_;
    bool executed_;
    NormalizedSql normalizedSql_;
    std::vector<BoundValue> boundValues_;
    MYSQL_RES* result_ = nullptr;
    MYSQL_ROW currentRow_ = nullptr;
    unsigned long* currentLengths_ = nullptr;
    my_ulonglong rowIndex_;
    my_ulonglong rowCount_ = 0;
};

class MariaDbTransaction final : public ITransaction {
public:
    explicit MariaDbTransaction(MYSQL* handle)
        : handle_{handle}
        , done_{false} {
        Execute("START TRANSACTION");
    }

    ~MariaDbTransaction() override {
        if (!done_) {
            try {
                Rollback();
            } catch (...) {
            }
        }
    }

    void Commit() override {
        Execute("COMMIT");
        done_ = true;
    }

    void Rollback() override {
        Execute("ROLLBACK");
        done_ = true;
    }

private:
    void Execute(const char* sql) {
        if (mysql_query(handle_, sql) != 0) {
            throw DatabaseException("mariadb transaction failed: " + std::string(mysql_error(handle_)));
        }
    }

    MYSQL* handle_;
    bool done_;
};

} // namespace

struct MariaDbDatabaseConnection::Impl {
    MYSQL* handle = nullptr;
};

MariaDbDatabaseConnection::MariaDbDatabaseConnection(const std::string& host, uint16_t port,
    const std::string& user, const std::string& password, const std::string& schema)
    : impl_{std::make_unique<Impl>()} {
    impl_->handle = mysql_init(nullptr);
    if (!impl_->handle) {
        throw DatabaseException("mariadb init failed");
    }

    if (!mysql_real_connect(impl_->handle, host.c_str(), user.c_str(), password.c_str(),
            schema.empty() ? nullptr : schema.c_str(), port, nullptr, 0)) {
        std::string error = mysql_error(impl_->handle);
        mysql_close(impl_->handle);
        impl_->handle = nullptr;
        throw DatabaseException("mariadb connect failed: " + error);
    }

    if (mysql_set_character_set(impl_->handle, "utf8mb4") != 0) {
        throw DatabaseException("mariadb set charset failed: " + std::string(mysql_error(impl_->handle)));
    }

    if (mysql_query(impl_->handle,
            "SET SESSION sql_mode = CONCAT_WS(',', @@sql_mode, 'PIPES_AS_CONCAT')")
        != 0) {
        throw DatabaseException("mariadb set sql_mode failed: " + std::string(mysql_error(impl_->handle)));
    }
}

MariaDbDatabaseConnection::~MariaDbDatabaseConnection() {
    if (impl_ && impl_->handle) {
        mysql_close(impl_->handle);
    }
}

std::unique_ptr<IStatement> MariaDbDatabaseConnection::Prepare(const std::string& sql) {
    return std::make_unique<MariaDbStatement>(impl_->handle, sql);
}

std::unique_ptr<ITransaction> MariaDbDatabaseConnection::BeginTransaction() {
    return std::make_unique<MariaDbTransaction>(impl_->handle);
}

uint64_t MariaDbDatabaseConnection::GetLastInsertId() const {
    return static_cast<uint64_t>(mysql_insert_id(impl_->handle));
}
