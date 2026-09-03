#include "lcpush/tokens.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "lcpush/keyring.hpp"
#include "lcpush/paths.hpp"
#include "lcpush/util/strings.hpp"
#include "lcpush/util/subprocess.hpp"

namespace lcpush::tokens {

namespace {

constexpr const char* kEnvVars[] = {"LCPUSH_GITHUB_TOKEN", "GITHUB_TOKEN"};

}  // namespace

std::optional<ResolvedToken> from_env() {
    for (const char* name : kEnvVars) {
        const char* raw = std::getenv(name);
        if (raw == nullptr) continue;
        std::string value = util::trim(raw);
        if (!value.empty()) {
            return ResolvedToken{value, std::string("$") + name};
        }
    }
    return std::nullopt;
}

std::optional<ResolvedToken> from_keyring() {
    keyring::Keyring* backend = keyring::keyring();
    if (backend == nullptr) return std::nullopt;
    auto value = backend->get(kService, kAccount);
    if (value && !value->empty()) {
        return ResolvedToken{util::trim(*value), "keyring"};
    }
    return std::nullopt;
}

std::optional<ResolvedToken> from_gh_cli() {
    auto& proc = util::subprocess();
    if (!proc.which("gh")) return std::nullopt;
    util::RunResult result = proc.run_capture({"gh", "auth", "token"}, 10);
    if (!result.ok || result.exit_code != 0) return std::nullopt;
    std::string value = util::trim(result.out);
    if (value.empty()) return std::nullopt;
    return ResolvedToken{value, "gh auth token"};
}

std::optional<ResolvedToken> from_file() {
    auto path = paths::token_file();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return std::nullopt;
    std::ifstream in(path);
    if (!in.good()) return std::nullopt;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string value = util::trim(buffer.str());
    if (value.empty()) return std::nullopt;
    return ResolvedToken{value, path.string()};
}

std::optional<ResolvedToken> resolve() {
    if (auto found = from_env()) return found;
    if (auto found = from_keyring()) return found;
    if (auto found = from_gh_cli()) return found;
    if (auto found = from_file()) return found;
    return std::nullopt;
}

std::string store(const std::string& token) {
    std::string value = util::trim(token);
    keyring::Keyring* backend = keyring::keyring();
    if (backend != nullptr && backend->set(kService, kAccount, value)) {
        return "keyring";
    }
    auto path = paths::token_file();
    paths::write_private(path, value + "\n");
    return path.string();
}

std::vector<std::string> clear() {
    std::vector<std::string> cleared;
    keyring::Keyring* backend = keyring::keyring();
    if (backend != nullptr) {
        auto existing = backend->get(kService, kAccount);
        if (existing && !existing->empty() && backend->remove(kService, kAccount)) {
            cleared.push_back("keyring");
        }
    }
    auto path = paths::token_file();
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && std::filesystem::remove(path, ec)) {
        cleared.push_back(path.string());
    }
    return cleared;
}

std::string redact(const std::string& text, const std::optional<std::string>& token) {
    if (!token || token->size() < 8) return text;
    return util::replace_all(text, *token, "****");
}

}  // namespace lcpush::tokens
