// First-run setup: repo, token, write-access verification.
#pragma once

#include <optional>
#include <string>
#include <utility>

#include "lcpush/config.hpp"
#include "lcpush/flow.hpp"
#include "lcpush/github.hpp"

namespace lcpush::onboarding {

// Find a token, prompting (hidden) as the last resort.
std::string resolve_token(const flow::Deps& deps, bool interactive, bool announce = false);

// Verify push access and read the default branch.
github::RepoInfo verify_repo(const flow::Deps& deps, const std::string& token,
                             const std::string& owner, const std::string& name);

// Run the first-run flow and return the saved config plus the token.
std::pair<config::Config, std::string> setup(
    const flow::Deps& deps, const std::optional<config::Config>& existing = std::nullopt);

}  // namespace lcpush::onboarding
