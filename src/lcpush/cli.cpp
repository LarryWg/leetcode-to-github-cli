#include "lcpush/cli.hpp"

#include <CLI/CLI.hpp>

#include <cstdlib>

#include "lcpush/config.hpp"
#include "lcpush/errors.hpp"
#include "lcpush/onboarding.hpp"
#include "lcpush/paths.hpp"
#include "lcpush/session.hpp"
#include "lcpush/tokens.hpp"
#include "lcpush/ui.hpp"
#include "lcpush/util/strings.hpp"
#include "lcpush/version.hpp"

namespace lcpush::cli {

namespace {

void echo(const std::string& text) {
    std::fputs(text.c_str(), ui::out_stream());
    std::fputc('\n', ui::out_stream());
    std::fflush(ui::out_stream());
}

config::Config require_config() {
    auto config = config::load();
    if (!config) throw ConfigError("No config yet. Run lcpush once to set up a repo.");
    return *config;
}

void config_show() {
    config::Config config = require_config();
    auto found = tokens::resolve();
    echo(config::render_show(config, found.has_value(), found ? found->source : ""));
}

// Set a config key. `repo` re-verifies write access before saving.
void config_set(const flow::Deps& deps, const std::string& key, const std::string& value) {
    config::Config config = config::load().value_or(config::Config{});
    config::Config updated = config::set_value(config, key, value);
    if (key == "repo") {
        auto [owner, name] = config::parse_repo(value);
        std::string token = onboarding::resolve_token(deps, deps.stdin_isatty());
        github::RepoInfo info = onboarding::verify_repo(deps, token, owner, name);
        ui::success("Verified write access to " + owner + "/" + name);
        if (!config.repo.configured()) {
            updated = config::set_value(updated, "branch", info.default_branch);
        }
    }
    auto path = config::save(updated);
    ui::success("Updated " + key + " in " + path.string());
}

// Forget the stored GitHub token (keyring and fallback file).
void config_reset_token() {
    auto cleared = tokens::clear();
    if (!cleared.empty()) {
        ui::success("Cleared stored token from: " + util::join(cleared, ", "));
    } else {
        ui::info("No stored token to clear.");
    }
    std::vector<std::string> leftover;
    for (const char* name : {"LCPUSH_GITHUB_TOKEN", "GITHUB_TOKEN"}) {
        if (std::getenv(name) != nullptr) leftover.push_back(std::string("$") + name);
    }
    if (!leftover.empty()) {
        ui::warn("⚠ " + util::join(leftover, ", ") +
                 " is still set in the environment and takes precedence.");
    }
}

void config_path() {
    echo("config: " + paths::config_file().string());
    echo("cache:  " + paths::problems_cache_file().string());
}

}  // namespace

int run(const std::vector<std::string>& args, const flow::Deps& deps) {
    CLI::App app{"Push a LeetCode solution to a GitHub repo in one interactive session."};
    app.name("lcpush");

    bool refresh = false;
    bool version = false;
    app.add_flag("--refresh", refresh, "Re-fetch the LeetCode problem list.");
    app.add_flag("--version", version, "Print the version and exit.");

    CLI::App* config_cmd =
        app.add_subcommand("config", "Inspect and change lcpush configuration.");
    config_cmd->require_subcommand(1);

    CLI::App* show_cmd = config_cmd->add_subcommand(
        "show", "Print the current config; the token is described, never printed.");
    std::string set_key;
    std::string set_value;
    CLI::App* set_cmd = config_cmd->add_subcommand(
        "set", "Set a config key. `repo` re-verifies write access before saving.");
    set_cmd->add_option("key", set_key,
                        "One of: " + util::join(config::settable_keys(), ", "))
        ->required();
    set_cmd->add_option("value", set_value, "New value.")->required();
    CLI::App* reset_cmd = config_cmd->add_subcommand(
        "reset-token", "Forget the stored GitHub token (keyring and fallback file).");
    CLI::App* path_cmd = config_cmd->add_subcommand(
        "path", "Print where lcpush keeps its config and cache.");

    try {
        // CLI11 consumes the argument vector back to front.
        std::vector<std::string> reversed(args.rbegin(), args.rend());
        app.parse(reversed);
    } catch (const CLI::ParseError& exc) {
        return app.exit(exc);
    }

    if (version) {
        echo(std::string("lcpush ") + lcpush::version());
        return 0;
    }

    try {
        if (config_cmd->parsed()) {
            if (show_cmd->parsed()) config_show();
            if (set_cmd->parsed()) config_set(deps, set_key, set_value);
            if (reset_cmd->parsed()) config_reset_token();
            if (path_cmd->parsed()) config_path();
            return 0;
        }
        return session::run(deps, refresh);
    } catch (const LcpushError& exc) {
        ui::error(exc.what());
        return exc.exit_code;
    }
}

}  // namespace lcpush::cli
