#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <sys/stat.h>

#include <fstream>

#include "helpers/fixtures.hpp"
#include "lcpush/config/config.hpp"
#include "lcpush/core/errors.hpp"
#include "lcpush/platform/paths.hpp"

using namespace lcpush;

TEST_CASE("config defaults match spec") {
    config::Config cfg;
    CHECK(cfg.commit.message_template == "Add {id}. {title} ({language})");
    CHECK(cfg.commit.update_template == "Update {id}. {title} ({language})");
    CHECK(cfg.commit.prompt == "confirm");
    CHECK(cfg.cache.problems_ttl_days == 7);
    CHECK(cfg.repo.branch == "main");
}

TEST_CASE("parse repo accepts common forms") {
    auto value = GENERATE(as<std::string>{}, "user/leetcode-solutions",
                          "https://github.com/user/leetcode-solutions",
                          "git@github.com:user/leetcode-solutions.git",
                          "  user/leetcode-solutions/  ");
    auto [owner, name] = config::parse_repo(value);
    CHECK(owner == "user");
    CHECK(name == "leetcode-solutions");
}

TEST_CASE("parse repo rejects junk") {
    auto value = GENERATE(as<std::string>{}, "user", "a/b/c", "");
    CHECK_THROWS_AS(config::parse_repo(value), ConfigError);
}

TEST_CASE("config round trip") {
    testing::IsolatedDirs dirs;
    auto cfg = config::set_value(config::Config{}, "repo", "user/solutions");
    auto path = config::save(cfg);
    CHECK(path == paths::config_file());
    auto loaded = config::load();
    REQUIRE(loaded.has_value());
    CHECK(*loaded == cfg);
}

TEST_CASE("config file is 0600") {
    testing::IsolatedDirs dirs;
    config::save(config::Config{});
    struct stat info{};
    REQUIRE(::stat(paths::config_file().c_str(), &info) == 0);
    CHECK((info.st_mode & 0777) == 0600);
}

TEST_CASE("load returns none without a file") {
    testing::IsolatedDirs dirs;
    CHECK_FALSE(config::load().has_value());
}

TEST_CASE("load rejects broken toml") {
    testing::IsolatedDirs dirs;
    auto path = paths::config_file();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "this is not = = toml";
    CHECK_THROWS_AS(config::load(), ConfigError);
}

TEST_CASE("set repo and branch and path") {
    auto cfg = config::set_value(config::Config{}, "repo", "me/solutions");
    cfg = config::set_value(cfg, "branch", "trunk");
    cfg = config::set_value(cfg, "path", "solutions");
    CHECK(cfg.repo.full_name() == "me/solutions");
    CHECK(cfg.repo.branch == "trunk");
    CHECK(cfg.repo.path == "solutions/");
}

TEST_CASE("path prefix normalizes slashes") {
    CHECK(config::set_value(config::Config{}, "path", "/a/b/").repo.path == "a/b/");
    CHECK(config::set_value(config::Config{}, "path", "  ").repo.path == "");
}

TEST_CASE("set is immutable") {
    config::Config original;
    auto updated = config::set_value(original, "branch", "dev");
    CHECK(original.repo.branch == "main");
    CHECK(updated.repo.branch == "dev");
}

TEST_CASE("set commit prompt validates") {
    CHECK(config::set_value(config::Config{}, "commit.prompt", "always").commit.prompt ==
          "always");
    CHECK_THROWS_AS(config::set_value(config::Config{}, "commit.prompt", "sometimes"),
                    ConfigError);
}

TEST_CASE("set rejects unknown key") {
    CHECK_THROWS_AS(config::set_value(config::Config{}, "repo.owner", "me"), ConfigError);
}

TEST_CASE("set rejects empty template and branch") {
    CHECK_THROWS_AS(config::set_value(config::Config{}, "commit.message_template", "   "),
                    ConfigError);
    CHECK_THROWS_AS(config::set_value(config::Config{}, "branch", " "), ConfigError);
}

TEST_CASE("set ttl validates") {
    CHECK(config::set_value(config::Config{}, "cache.problems_ttl_days", "30")
              .cache.problems_ttl_days == 30);
    CHECK_THROWS_AS(config::set_value(config::Config{}, "cache.problems_ttl_days", "soon"),
                    ConfigError);
    CHECK_THROWS_AS(config::set_value(config::Config{}, "cache.problems_ttl_days", "-1"),
                    ConfigError);
}

TEST_CASE("load falls back on garbage values") {
    testing::IsolatedDirs dirs;
    auto path = paths::config_file();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "[cache]\nproblems_ttl_days = \"many\"\n"
                        << "[commit]\nprompt = \"x\"\n";
    auto cfg = config::load();
    REQUIRE(cfg.has_value());
    CHECK(cfg->cache.problems_ttl_days == 7);
    CHECK(cfg->commit.prompt == "confirm");
}

TEST_CASE("show never contains a token") {
    auto cfg = config::set_value(config::Config{}, "repo", "user/solutions");
    auto output = config::render_show(cfg, true, "keyring");
    CHECK(output.find("ghp_") == std::string::npos);
    CHECK(output.find("redacted") != std::string::npos);
    CHECK(output.find("user") != std::string::npos);
}

TEST_CASE("dumps matches the python tomli_w layout") {
    config::Config cfg;
    cfg.repo.owner = "user";
    cfg.repo.name = "leetcode-solutions";
    std::string expected =
        "[repo]\n"
        "owner = \"user\"\n"
        "name = \"leetcode-solutions\"\n"
        "branch = \"main\"\n"
        "path = \"\"\n"
        "\n"
        "[commit]\n"
        "message_template = \"Add {id}. {title} ({language})\"\n"
        "update_template = \"Update {id}. {title} ({language})\"\n"
        "prompt = \"confirm\"\n"
        "author_name = \"\"\n"
        "author_email = \"\"\n"
        "\n"
        "[cache]\n"
        "problems_ttl_days = 7\n";
    CHECK(config::dumps(cfg) == expected);
}
