#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

enum class StatementStepResult {
    Row,
    Done
};

class DatabaseException : public std::runtime_error {
public:
    explicit DatabaseException(const std::string& message)
        : std::runtime_error(message)
        , backend_{"database"}
        , code_{0} {}

    DatabaseException(std::string backend, int code, const std::string& message)
        : std::runtime_error("database error [" + backend + ":" + std::to_string(code) + "] " + message)
        , backend_{std::move(backend)}
        , code_{code} {}

    const std::string& Backend() const { return backend_; }
    int Code() const { return code_; }

private:
    std::string backend_;
    int code_;
};

class IStatement {
public:
    virtual ~IStatement() = default;

    virtual int BindParameterIndex(const std::string& name) const = 0;
    virtual void BindInt(int index, int64_t value) = 0;
    virtual void BindText(int index, const std::string& value) = 0;
    virtual void BindBlob(int index, const uint8_t* data, size_t length) = 0;

    virtual StatementStepResult Step() = 0;

    virtual int ColumnInt(int index) const = 0;
    virtual std::string ColumnText(int index) const = 0;
    virtual const uint8_t* ColumnBlob(int index) const = 0;
    virtual int ColumnBytes(int index) const = 0;
};

class ITransaction {
public:
    virtual ~ITransaction() = default;
    virtual void Commit() = 0;
    virtual void Rollback() = 0;
};

enum class UpsertStrategy {
    InsertIgnore,
    InsertOrIgnore,
    InsertOnConflictDoNothing
};

enum class BlobSemantics {
    NativeBlob,
    HexEncodedLiteral
};

enum class TransactionIsolationSupport {
    SerializableOnly,
    ReadCommitted
};

struct DatabaseCapabilities {
    UpsertStrategy upsertStrategy;
    BlobSemantics blobSemantics;
    TransactionIsolationSupport transactionIsolationSupport;
};

class IDatabaseConnection {
public:
    virtual ~IDatabaseConnection() = default;

    virtual std::unique_ptr<IStatement> Prepare(const std::string& sql) = 0;
    virtual std::unique_ptr<ITransaction> BeginTransaction() = 0;
    virtual uint64_t GetLastInsertId() const = 0;
    virtual std::string BackendName() const = 0;
    virtual const DatabaseCapabilities& Capabilities() const = 0;
};

class StatementHandle {
public:
    explicit StatementHandle(std::unique_ptr<IStatement> statement)
        : statement_{std::move(statement)} {}

    IStatement* operator->() { return statement_.get(); }
    const IStatement* operator->() const { return statement_.get(); }

    StatementStepResult Step() { return statement_->Step(); }

    void ExpectDone(const std::string& context = "statement") {
        if (statement_->Step() != StatementStepResult::Done) {
            throw DatabaseException("database", 0, context + " expected statement completion");
        }
    }

private:
    std::unique_ptr<IStatement> statement_;
};

class TransactionScope {
public:
    explicit TransactionScope(std::unique_ptr<ITransaction> transaction)
        : transaction_{std::move(transaction)} {}

    ~TransactionScope() {
        if (!committed_ && transaction_) {
            try {
                transaction_->Rollback();
            } catch (...) {
            }
        }
    }

    void Commit() {
        transaction_->Commit();
        committed_ = true;
    }

private:
    std::unique_ptr<ITransaction> transaction_;
    bool committed_ = false;
};
