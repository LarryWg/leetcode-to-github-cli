#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <fstream>
#include <sstream>

#include "helpers/capture.hpp"
#include "helpers/fake_subprocess.hpp"
#include "helpers/fixtures.hpp"
#include "lcpush/config/config.hpp"
#include "lcpush/core/errors.hpp"
#include "lcpush/app/onboarding.hpp"
#include "lcpush/platform/paths.hpp"

using namespace lcpush;
using lcpush::testing::CaptureStreams;
using lcpush::testing::EnvVar;
using lcpush::testing::FakeSubprocess;
using lcpush::testing::IsolatedDirs;

namespace {

// A github::Api scripted to answer get_repo.
class FakeRepoApi final : public github::Api {
  public:
    github::RepoInfo info{"user/solutions", "main", true};

    github::RepoInfo get_repo(const std::string&, const std::string&) override {
        return info;
    }
    std::optional<std::string> get_file_sha(const std::string&, const std::string&,
                                            const std::string&, const std::string&) override {
        return std::nullopt;
    }
    github::PushResult put_file(const std::string&, const std::string&, const std::string&,
                                const github::PutFileOptions&) override {
        return {};
    }
};

flow::Deps deps_with_repo(github::RepoInfo info) {
    flow::Deps deps;
    deps.stdin_isatty = [] { return true; };
    deps.github = [info](const std::string&) -> std::unique_ptr<github::Api> {
        auto api = std::make_unique<FakeRepoApi>();
        api->info = info;
        return api;
    };
    return deps;
}

// The tokens baseline: no keyring, no gh CLI.
struct NoBackends {
    keyring::KeyringOverride no_keyring{nullptr};
    FakeSubprocess proc;
    util::SubprocessOverride subprocess_override{&proc};
};

}  // namespace

TEST_CASE("resolve token prefers the environment") {
    IsolatedDirs dirs;
    EnvVar env("LCPUSH_GITHUB_TOKEN", "env-token");
    CHECK(onboarding::resolve_token(flow::Deps{}, false) == "env-token");
}

TEST_CASE("resolve token non interactive without one") {
    IsolatedDirs dirs;
    NoBackends backends;
    CHECK_THROWS_MATCHES(onboarding::resolve_token(flow::Deps{}, false), TokenError,
                         Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring(
                             "LCPUSH_GITHUB_TOKEN")));
}

TEST_CASE("resolve token prompts and stores") {
    IsolatedDirs dirs;
    NoBackends backends;
    CaptureStreams capture;
    flow::Deps deps;
    deps.password = [](const std::string&) { return " ghp_typed "; };
    CHECK(onboarding::resolve_token(deps, true) == "ghp_typed");

    std::ifstream in(paths::token_file());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    CHECK(buffer.str() == "ghp_typed\n");
    CHECK(capture.err().find("No OS keyring available") != std::string::npos);
}

TEST_CASE("resolve token rejects an empty prompt") {
    IsolatedDirs dirs;
    NoBackends backends;
    CaptureStreams capture;
    flow::Deps deps;
    deps.password = [](const std::string&) { return "   "; };
    CHECK_THROWS_AS(onboarding::resolve_token(deps, true), TokenError);
}

TEST_CASE("verify repo rejects read only tokens") {
    auto deps = deps_with_repo({"user/solutions", "main", false});
    CHECK_THROWS_MATCHES(onboarding::verify_repo(deps, "t", "user", "solutions"), TokenError,
                         Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring(
                             "contents: read & write")));
}

TEST_CASE("verify repo returns the default branch") {
    auto deps = deps_with_repo({"user/solutions", "trunk", true});
    CHECK(onboarding::verify_repo(deps, "t", "user", "solutions").default_branch == "trunk");
}

TEST_CASE("setup persists repo and resolved branch") {
    IsolatedDirs dirs;
    EnvVar env("LCPUSH_GITHUB_TOKEN", "env-token");
    CaptureStreams capture;
    auto deps = deps_with_repo({"user/solutions", "trunk", true});
    deps.text = [](const std::string&, const std::string&) { return "user/solutions"; };

    auto [config, token] = onboarding::setup(deps);

    CHECK(token == "env-token");
    CHECK(config.repo.full_name() == "user/solutions");
    CHECK(config.repo.branch == "trunk");
    auto loaded = config::load();
    REQUIRE(loaded.has_value());
    CHECK(*loaded == config);

    std::string out = capture.out();
    CHECK(out.find("Verified write access to user/solutions") != std::string::npos);
    CHECK(out.find("Saved config to") != std::string::npos);
    CHECK(out.find("env-token") == std::string::npos);
}

TEST_CASE("setup requires a repo") {
    IsolatedDirs dirs;
    CaptureStreams capture;
    flow::Deps deps;
    deps.text = [](const std::string&, const std::string&) { return "  "; };
    CHECK_THROWS_AS(onboarding::setup(deps, config::Config{}), ConfigError);
}
