// CLI surface behaviour: flags, config subcommands, error handling.
#include <catch2/catch_test_macros.hpp>

#include "helpers/capture.hpp"
#include "helpers/fixtures.hpp"
#include "lcpush/cli.hpp"
#include "lcpush/config.hpp"
#include "lcpush/problems.hpp"

using namespace lcpush;
using lcpush::testing::CaptureStreams;
using lcpush::testing::EnvVar;
using lcpush::testing::IsolatedDirs;

namespace {

// A configured, token-bearing, warm-cache environment.
struct Ready {
    IsolatedDirs dirs;
    EnvVar token_env{"LCPUSH_GITHUB_TOKEN", "ghp_test_token_value"};
    CaptureStreams capture;
    flow::Deps deps;

    Ready() {
        config::save(config::set_value(config::Config{}, "repo", "user/leetcode-solutions"));
        problems::save_cache(testing::questions());
        deps.stdin_isatty = [] { return true; };
    }

    std::string output() const { return capture.out() + capture.err(); }
};

}  // namespace

TEST_CASE("cli version") {
    IsolatedDirs dirs;
    CaptureStreams capture;
    CHECK(cli::run({"--version"}, flow::Deps{}) == 0);
    CHECK(capture.out().find("lcpush") != std::string::npos);
}

TEST_CASE("cli piped input is refused") {
    Ready ready;
    ready.deps.stdin_isatty = [] { return false; };
    CHECK(cli::run({}, ready.deps) == 1);
    CHECK(ready.output().find("terminal") != std::string::npos);
}

TEST_CASE("cli unknown flag is rejected") {
    Ready ready;
    CHECK(cli::run({"--slug", "two-sum"}, ready.deps) != 0);
}

TEST_CASE("cli config show redacts the token") {
    Ready ready;
    CHECK(cli::run({"config", "show"}, ready.deps) == 0);
    std::string out = ready.capture.out();
    CHECK(out.find("ghp_test_token_value") == std::string::npos);
    CHECK(out.find("redacted") != std::string::npos);
    CHECK(out.find("leetcode-solutions") != std::string::npos);
}

TEST_CASE("cli config show without config") {
    IsolatedDirs dirs;
    CaptureStreams capture;
    CHECK(cli::run({"config", "show"}, flow::Deps{}) == 1);
    CHECK(capture.err().find("No config yet") != std::string::npos);
}

TEST_CASE("cli config set branch") {
    Ready ready;
    CHECK(cli::run({"config", "set", "branch", "trunk"}, ready.deps) == 0);
    auto loaded = config::load();
    REQUIRE(loaded.has_value());
    CHECK(loaded->repo.branch == "trunk");
}

TEST_CASE("cli config set rejects unknown key") {
    Ready ready;
    CHECK(cli::run({"config", "set", "nope", "x"}, ready.deps) == 1);
    CHECK(ready.output().find("Unknown config key") != std::string::npos);
}

TEST_CASE("cli config reset token warns about env") {
    Ready ready;
    CHECK(cli::run({"config", "reset-token"}, ready.deps) == 0);
    CHECK(ready.output().find("LCPUSH_GITHUB_TOKEN") != std::string::npos);
}

TEST_CASE("cli config path prints locations") {
    Ready ready;
    CHECK(cli::run({"config", "path"}, ready.deps) == 0);
    std::string out = ready.capture.out();
    CHECK(out.find("config:") != std::string::npos);
    CHECK(out.find("cache:") != std::string::npos);
}
