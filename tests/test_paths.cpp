#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

#include "helpers/fixtures.hpp"
#include "lcpush/paths.hpp"

using namespace lcpush;
using lcpush::testing::EnvVar;
using lcpush::testing::IsolatedDirs;

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

}  // namespace

TEST_CASE("defaults to dot config and dot cache") {
    IsolatedDirs dirs;
    EnvVar no_config("LCPUSH_CONFIG_DIR", std::nullopt);
    EnvVar no_cache("LCPUSH_CACHE_DIR", std::nullopt);
    EnvVar no_xdg_config("XDG_CONFIG_HOME", std::nullopt);
    EnvVar no_xdg_cache("XDG_CACHE_HOME", std::nullopt);
    EnvVar home("HOME", dirs.root().string());
    CHECK(paths::config_dir() == dirs.root() / ".config" / "lcpush");
    CHECK(paths::cache_dir() == dirs.root() / ".cache" / "lcpush");
}

TEST_CASE("honours xdg variables") {
    IsolatedDirs dirs;
    EnvVar no_config("LCPUSH_CONFIG_DIR", std::nullopt);
    EnvVar no_cache("LCPUSH_CACHE_DIR", std::nullopt);
    EnvVar xdg_config("XDG_CONFIG_HOME", (dirs.root() / "cfg").string());
    EnvVar xdg_cache("XDG_CACHE_HOME", (dirs.root() / "cch").string());
    CHECK(paths::config_dir() == dirs.root() / "cfg" / "lcpush");
    CHECK(paths::cache_dir() == dirs.root() / "cch" / "lcpush");
}

TEST_CASE("explicit overrides win") {
    IsolatedDirs dirs;
    EnvVar xdg("XDG_CONFIG_HOME", (dirs.root() / "ignored").string());
    EnvVar explicit_dir("LCPUSH_CONFIG_DIR", (dirs.root() / "explicit").string());
    CHECK(paths::config_dir() == dirs.root() / "explicit");
}

TEST_CASE("file names") {
    IsolatedDirs dirs;
    CHECK(paths::config_file().filename() == "config.toml");
    CHECK(paths::token_file().filename() == "token");
    CHECK(paths::problems_cache_file().filename() == "problems.json");
    CHECK(paths::problems_cache_file().parent_path() == paths::cache_dir());
}

TEST_CASE("write private creates parents") {
    IsolatedDirs dirs;
    auto target = dirs.root() / "deep" / "nested" / "file";
    paths::write_private(target, "secret\n");
    CHECK(read_file(target) == "secret\n");
}

TEST_CASE("write private truncates an existing file") {
    IsolatedDirs dirs;
    auto target = dirs.root() / "file";
    paths::write_private(target, "a long previous value\n");
    paths::write_private(target, "new\n");
    CHECK(read_file(target) == "new\n");
}
