#include <catch2/catch_test_macros.hpp>

#include <sys/stat.h>

#include "helpers/fake_subprocess.hpp"
#include "helpers/fixtures.hpp"
#include "lcpush/platform/paths.hpp"
#include "lcpush/github/tokens.hpp"

using namespace lcpush;
using lcpush::testing::EnvVar;
using lcpush::testing::FakeKeyring;
using lcpush::testing::FakeSubprocess;
using lcpush::testing::IsolatedDirs;

namespace {

// Most token tests want "no keyring, no gh CLI" as the baseline.
struct NoBackends {
    keyring::KeyringOverride no_keyring{nullptr};
    FakeSubprocess proc;
    util::SubprocessOverride subprocess_override{&proc};
};

}  // namespace

TEST_CASE("env var precedence") {
    IsolatedDirs dirs;
    EnvVar github("GITHUB_TOKEN", "from-github-token");
    EnvVar lcpush_var("LCPUSH_GITHUB_TOKEN", "from-lcpush-token");
    auto found = tokens::resolve();
    REQUIRE(found.has_value());
    CHECK(found->value == "from-lcpush-token");
    CHECK(found->source == "$LCPUSH_GITHUB_TOKEN");
}

TEST_CASE("env fallback to github token") {
    IsolatedDirs dirs;
    EnvVar github("GITHUB_TOKEN", "gh-token");
    auto found = tokens::from_env();
    REQUIRE(found.has_value());
    CHECK(found->source == "$GITHUB_TOKEN");
}

TEST_CASE("blank env is ignored") {
    IsolatedDirs dirs;
    EnvVar blank("LCPUSH_GITHUB_TOKEN", "   ");
    CHECK_FALSE(tokens::from_env().has_value());
}

TEST_CASE("file fallback is 0600") {
    IsolatedDirs dirs;
    NoBackends backends;
    auto where = tokens::store("secret-value");
    CHECK(where == paths::token_file().string());
    struct stat info{};
    REQUIRE(::stat(paths::token_file().c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0600);
    auto found = tokens::from_file();
    REQUIRE(found.has_value());
    CHECK(found->value == "secret-value");
}

TEST_CASE("keyring is preferred") {
    IsolatedDirs dirs;
    FakeKeyring fake;
    keyring::KeyringOverride keyring_override(&fake);
    CHECK(tokens::store("kr-token") == "keyring");
    auto found = tokens::from_keyring();
    REQUIRE(found.has_value());
    CHECK(found->value == "kr-token");
    CHECK(tokens::clear() == std::vector<std::string>{"keyring"});
    CHECK_FALSE(tokens::from_keyring().has_value());
}

TEST_CASE("keyring failure falls back to file") {
    IsolatedDirs dirs;
    FakeKeyring broken;
    broken.broken = true;
    keyring::KeyringOverride keyring_override(&broken);
    CHECK(tokens::store("fallback") == paths::token_file().string());
    CHECK_FALSE(tokens::from_keyring().has_value());
}

TEST_CASE("gh cli used when present") {
    IsolatedDirs dirs;
    keyring::KeyringOverride no_keyring(nullptr);
    FakeSubprocess proc;
    proc.on_which = [](const std::string&) { return std::optional<std::string>{"/usr/bin/gh"}; };
    proc.on_run_capture = [](const std::vector<std::string>&) {
        return util::RunResult{0, "gho_from_cli\n", true};
    };
    util::SubprocessOverride subprocess_override(&proc);
    auto found = tokens::resolve();
    REQUIRE(found.has_value());
    CHECK(found->value == "gho_from_cli");
    CHECK(found->source == "gh auth token");
}

TEST_CASE("gh cli absent") {
    IsolatedDirs dirs;
    NoBackends backends;
    CHECK_FALSE(tokens::from_gh_cli().has_value());
}

TEST_CASE("resolve returns none when nothing stored") {
    IsolatedDirs dirs;
    NoBackends backends;
    CHECK_FALSE(tokens::resolve().has_value());
}

TEST_CASE("clear removes the file") {
    IsolatedDirs dirs;
    NoBackends backends;
    tokens::store("bye");
    CHECK(tokens::clear() == std::vector<std::string>{paths::token_file().string()});
    CHECK_FALSE(std::filesystem::exists(paths::token_file()));
}

TEST_CASE("redact") {
    CHECK(tokens::redact("url ghp_abcdefgh here", std::string("ghp_abcdefgh")) ==
          "url **** here");
    CHECK(tokens::redact("nothing", std::nullopt) == "nothing");
    CHECK(tokens::redact("short", std::string("abc")) == "short");
}
