// GitHub token resolution and storage.
// Resolution order, first hit wins: $LCPUSH_GITHUB_TOKEN, $GITHUB_TOKEN,
// OS keyring, `gh auth token`, the token fallback file.
// The token is never written to config.toml and never echoed back.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lcpush::tokens {

inline constexpr const char* kService = "lcpush";
inline constexpr const char* kAccount = "github";

struct ResolvedToken {
    std::string value;
    std::string source;
};

std::optional<ResolvedToken> from_env();
std::optional<ResolvedToken> from_keyring();
std::optional<ResolvedToken> from_gh_cli();
std::optional<ResolvedToken> from_file();

// First hit wins across every source, or nullopt if nothing is stored.
std::optional<ResolvedToken> resolve();

// Persist a token, preferring the keyring. Returns "keyring" or the fallback
// file path so the caller can surface the file case as a warning.
std::string store(const std::string& token);

// Remove every token lcpush itself stored. Returns what was cleared.
std::vector<std::string> clear();

// Scrub a token out of any string headed for a terminal or a log.
std::string redact(const std::string& text, const std::optional<std::string>& token);

}  // namespace lcpush::tokens
