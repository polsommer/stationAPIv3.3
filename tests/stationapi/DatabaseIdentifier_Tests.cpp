#include "catch.hpp"

#include "Database.hpp"

namespace {

class BackendOnlyConnection final : public IDatabaseConnection {
public:
    explicit BackendOnlyConnection(std::string backendName)
        : backendName_{std::move(backendName)} {}

    std::unique_ptr<IStatement> Prepare(const std::string&) override { throw std::runtime_error("unused"); }
    std::unique_ptr<ITransaction> BeginTransaction() override { throw std::runtime_error("unused"); }
    uint64_t GetLastInsertId() const override { return 0; }
    std::string BackendName() const override { return backendName_; }
    const DatabaseCapabilities& Capabilities() const override { return capabilities_; }

private:
    std::string backendName_;
    DatabaseCapabilities capabilities_{UpsertStrategy::InsertIgnore, BlobSemantics::NativeBlob,
        TransactionIsolationSupport::SerializableOnly};
};

} // namespace

SCENARIO("ignore table identifier is backend-safe", "[database]") {
    REQUIRE(IgnoreTableIdentifierForBackend("sqlite") == "ignore");
    REQUIRE(IgnoreTableIdentifierForBackend("mariadb") == "`ignore`");
}

SCENARIO("ignore table identifier can be resolved from a database connection", "[database]") {
    BackendOnlyConnection sqliteDb{"sqlite"};
    BackendOnlyConnection mariaDb{"mariadb"};

    REQUIRE(IgnoreTableIdentifier(sqliteDb) == "ignore");
    REQUIRE(IgnoreTableIdentifier(mariaDb) == "`ignore`");
}
