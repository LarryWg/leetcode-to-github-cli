// Config file model and TOML persistence. The config is an immutable value
// object: set_value returns a new Config, so a failed set never leaves a
// half-applied config behind. The token is never part of this model.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace lcpush::config {

inline constexpr const char* kDefaultMessageTemplate = "Add {id}. {title} ({language})";
inline constexpr const char* kDefaultUpdateTemplate = "Update {id}. {title} ({language})";

const std::vector<std::string>& prompt_modes();

struct RepoConfig {
    std::string owner;
    std::string name;
    std::string branch = "main";
    std::string path;

    std::string full_name() const { return owner + "/" + name; }
    bool configured() const { return !owner.empty() && !name.empty(); }

    bool operator==(const RepoConfig&) const = default;
};

struct CommitConfig {
    std::string message_template = kDefaultMessageTemplate;
    std::string update_template = kDefaultUpdateTemplate;
    std::string prompt = "confirm";
    std::string author_name;
    std::string author_email;

    bool operator==(const CommitConfig&) const = default;
};

struct CacheConfig {
    int problems_ttl_days = 7;

    bool operator==(const CacheConfig&) const = default;
};

struct Config {
    RepoConfig repo;
    CommitConfig commit;
    CacheConfig cache;

    bool operator==(const Config&) const = default;
};

// Parse owner/name, also accepting a full GitHub URL.
std::pair<std::string, std::string> parse_repo(const std::string& value);

// A non-empty subdirectory prefix always ends in exactly one slash.
std::string normalize_path_prefix(const std::string& value);

// Serialize in the exact layout the Python tomli_w writer produced.
std::string dumps(const Config& config);

// The stored config, or nullopt when there is no config file yet.
std::optional<Config> load();
std::optional<Config> load(const std::filesystem::path& path);

// Persist with mode 0600 and return where it was written.
std::filesystem::path save(const Config& config);
std::filesystem::path save(const Config& config, const std::filesystem::path& path);

const std::vector<std::string>& settable_keys();

// A new Config with key set to value. Throws ConfigError on unknown keys or
// invalid values.
Config set_value(const Config& config, const std::string& key, const std::string& value);

// Human-readable config dump. The token is described, never printed.
std::string render_show(const Config& config, bool token_present,
                        const std::string& token_source = "");

}  // namespace lcpush::config
