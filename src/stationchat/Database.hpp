#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

enum class StatementStepResult {
    Row,
    Done
};

class DatabaseException : public std::runtime_error {
public:
    explicit DatabaseException(const std::string& message)
        : std::runtime_error(message) {}
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

class IDatabaseConnection {
public:
    virtual ~IDatabaseConnection() = default;

    virtual std::unique_ptr<IStatement> Prepare(const std::string& sql) = 0;
    virtual std::unique_ptr<ITransaction> BeginTransaction() = 0;
    virtual uint64_t GetLastInsertId() const = 0;
};
