#include "lcpush/app/onboarding.hpp"

#include "lcpush/core/errors.hpp"
#include "lcpush/platform/paths.hpp"
#include "lcpush/github/tokens.hpp"
#include "lcpush/ui/console.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::onboarding {

std::string resolve_token(const flow::Deps& deps, bool interactive, bool announce) {
    auto found = tokens::resolve();
    if (found) {
        if (announce) ui::dim("  Using GitHub token from " + found->source);
        return found->value;
    }
    if (!interactive) {
        throw TokenError(
            "No GitHub token found. Set $LCPUSH_GITHUB_TOKEN, or run lcpush "
            "interactively once to store one.");
    }
    ui::dim("  No stored GitHub token found.");
    std::string value = util::trim(deps.password("? GitHub token:"));
    if (value.empty()) throw TokenError("No GitHub token provided.");
    std::string where = tokens::store(value);
    if (where == "keyring") {
        ui::success("Saved token to the OS keyring");
    } else {
        ui::warn("⚠ No OS keyring available; stored the token at " +
                 paths::token_file().string() + " (mode 0600).");
    }
    return value;
}

github::RepoInfo verify_repo(const flow::Deps& deps, const std::string& token,
                             const std::string& owner, const std::string& name) {
    auto client = deps.github(token);
    github::RepoInfo info = client->get_repo(owner, name);
    if (!info.can_push) {
        throw TokenError("Token cannot write to " + owner + "/" + name +
                         ". Needs the `repo` scope (classic) or `contents: read & write` "
                         "(fine-grained).");
    }
    return info;
}

std::pair<config::Config, std::string> setup(const flow::Deps& deps,
                                             const std::optional<config::Config>& existing) {
    config::Config config = existing.value_or(config::Config{});

    ui::info("");
    std::string answer = util::trim(deps.text("? GitHub repo to push to:", ""));
    if (answer.empty()) throw ConfigError("A target repo is required.");
    auto [owner, name] = config::parse_repo(answer);

    std::string token = resolve_token(deps, true, true);
    github::RepoInfo info = verify_repo(deps, token, owner, name);
    ui::success("Verified write access to " + owner + "/" + name);

    config.repo.owner = owner;
    config.repo.name = name;
    config.repo.branch = info.default_branch;
    auto path = config::save(config);
    ui::success("Saved config to " + path.string());
    return {config, token};
}

}  // namespace lcpush::onboarding
