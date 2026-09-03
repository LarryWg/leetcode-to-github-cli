#include <catch2/catch_test_macros.hpp>

#include "lcpush/errors.hpp"

using namespace lcpush;

TEST_CASE("errors carry messages and exit codes") {
    ConfigError config_error("bad config");
    CHECK(std::string(config_error.what()) == "bad config");
    CHECK(config_error.exit_code == 1);

    Cancelled cancelled;
    CHECK(std::string(cancelled.what()) == "Aborted.");
    CHECK(cancelled.exit_code == 130);

    Cancelled custom("Cancelled — nothing was pushed.");
    CHECK(std::string(custom.what()) == "Cancelled — nothing was pushed.");
    CHECK(custom.exit_code == 130);
}

TEST_CASE("subclasses are catchable as LcpushError") {
    try {
        throw TokenError("no token");
    } catch (const LcpushError& err) {
        CHECK(std::string(err.what()) == "no token");
        CHECK(err.exit_code == 1);
    }
}
