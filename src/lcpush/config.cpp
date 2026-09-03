#include "lcpush/config.hpp"

#include <toml++/toml.hpp>

#include <cmath>
#include <cstdio>

#include "lcpush/errors.hpp"
#include "lcpush/paths.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::config {

namespace {

// Coerce any scalar to a string the way Python str() would.
std::string as_string(const toml::node* node, const std::string& fallback) {
    if (node == nullptr) return fallback;
    if (auto value = node->value<std::string>()) return *value;
    if (node->is_boolean()) return node->as_boolean()->get() ? "True" : "False";
    if (node->is_integer()) return std::to_string(node->as_integer()->get());
    if (node->is_floating_point()) {
        double value = node->as_floating_point()->get();
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%g", value);
        std::string text(buffer);
        if (text.find('.') == std::string::npos && text.find('e') == std::string::npos &&
            text.find("inf") == std::string::npos && text.find("nan") == std::string::npos) {
            text += ".0";
        }
        return text;
    }
    return fallback;
}

int as_ttl(const toml::node* node, int fallback) {
    if (node == nullptr) return fallback;
    if (node->is_integer()) return static_cast<int>(node->as_integer()->get());
    if (node->is_floating_point()) {
        return static_cast<int>(node->as_floating_point()->get());
    }
    if (auto text = node->value<std::string>()) {
        try {
            size_t used = 0;
            std::string trimmed = util::trim(*text);
            int value = std::stoi(trimmed, &used);
            if (used == trimmed.size()) return value;
        } catch (const std::exception&) {
        }
    }
    return fallback;
}

Config from_table(const toml::table& data) {
    const toml::table* repo_raw = data["repo"].as_table();
    const toml::table* commit_raw = data["commit"].as_table();
    const toml::table* cache_raw = data["cache"].as_table();
    auto field = [](const toml::table* table, const char* key) -> const toml::node* {
        return table ? table->get(key) : nullptr;
    };

    CommitConfig commit;
    commit.message_template =
        as_string(field(commit_raw, "message_template"), kDefaultMessageTemplate);
    commit.update_template =
        as_string(field(commit_raw, "update_template"), kDefaultUpdateTemplate);
    commit.prompt = as_string(field(commit_raw, "prompt"), "confirm");
    bool known_mode = false;
    for (const std::string& mode : prompt_modes()) {
        if (commit.prompt == mode) known_mode = true;
    }
    if (!known_mode) commit.prompt = "confirm";
    commit.author_name = as_string(field(commit_raw, "author_name"), "");
    commit.author_email = as_string(field(commit_raw, "author_email"), "");

    RepoConfig repo;
    repo.owner = as_string(field(repo_raw, "owner"), "");
    repo.name = as_string(field(repo_raw, "name"), "");
    repo.branch = as_string(field(repo_raw, "branch"), "");
    if (repo.branch.empty()) repo.branch = "main";
    repo.path = normalize_path_prefix(as_string(field(repo_raw, "path"), ""));

    CacheConfig cache;
    cache.problems_ttl_days = as_ttl(field(cache_raw, "problems_ttl_days"), 7);

    return Config{repo, commit, cache};
}

// TOML basic-string escaping matching tomli_w output.
std::string quoted(const std::string& text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\t': out += "\\t"; break;
            case '\n': out += "\\n"; break;
            case '\f': out += "\\f"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c < 0x20 || c == 0x7f) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04X", c);
                    out += buffer;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += "\"";
    return out;
}

}  // namespace

const std::vector<std::string>& prompt_modes() {
    static const std::vector<std::string> modes = {"confirm", "always", "never"};
    return modes;
}

std::pair<std::string, std::string> parse_repo(const std::string& value) {
    std::string text = util::rstrip(util::trim(value), '/');
    for (const char* prefix :
         {"https://github.com/", "http://github.com/", "git@github.com:"}) {
        if (util::starts_with(text, prefix)) {
            text = text.substr(std::string(prefix).size());
        }
    }
    if (util::ends_with(text, ".git")) {
        text = text.substr(0, text.size() - 4);
    }
    std::vector<std::string> parts;
    for (const std::string& part : util::split(text, '/')) {
        if (!part.empty()) parts.push_back(part);
    }
    if (parts.size() != 2) {
        throw ConfigError("Expected repo as owner/name, got: " + value);
    }
    return {parts[0], parts[1]};
}

std::string normalize_path_prefix(const std::string& value) {
    std::string text = util::strip_chars(util::trim(value), "/");
    return text.empty() ? "" : text + "/";
}

std::string dumps(const Config& config) {
    std::string out;
    out += "[repo]\n";
    out += "owner = " + quoted(config.repo.owner) + "\n";
    out += "name = " + quoted(config.repo.name) + "\n";
    out += "branch = " + quoted(config.repo.branch) + "\n";
    out += "path = " + quoted(config.repo.path) + "\n";
    out += "\n[commit]\n";
    out += "message_template = " + quoted(config.commit.message_template) + "\n";
    out += "update_template = " + quoted(config.commit.update_template) + "\n";
    out += "prompt = " + quoted(config.commit.prompt) + "\n";
    out += "author_name = " + quoted(config.commit.author_name) + "\n";
    out += "author_email = " + quoted(config.commit.author_email) + "\n";
    out += "\n[cache]\n";
    out += "problems_ttl_days = " + std::to_string(config.cache.problems_ttl_days) + "\n";
    return out;
}

std::optional<Config> load() { return load(paths::config_file()); }

std::optional<Config> load(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return std::nullopt;
    try {
        toml::table data = toml::parse_file(path.string());
        return from_table(data);
    } catch (const toml::parse_error& exc) {
        throw ConfigError("Could not read config at " + path.string() + ": " +
                          std::string(exc.description()));
    } catch (const std::exception& exc) {
        throw ConfigError("Could not read config at " + path.string() + ": " + exc.what());
    }
}

std::filesystem::path save(const Config& config) { return save(config, paths::config_file()); }

std::filesystem::path save(const Config& config, const std::filesystem::path& path) {
    paths::write_private(path, dumps(config));
    return path;
}

const std::vector<std::string>& settable_keys() {
    static const std::vector<std::string> keys = {
        "repo",
        "branch",
        "path",
        "commit.message_template",
        "commit.update_template",
        "commit.prompt",
        "commit.author_name",
        "commit.author_email",
        "cache.problems_ttl_days",
    };
    return keys;
}

Config set_value(const Config& config, const std::string& key, const std::string& value) {
    Config next = config;
    if (key == "repo") {
        auto [owner, name] = parse_repo(value);
        next.repo.owner = owner;
        next.repo.name = name;
        return next;
    }
    if (key == "branch") {
        std::string branch = util::trim(value);
        if (branch.empty()) throw ConfigError("Branch cannot be empty");
        next.repo.branch = branch;
        return next;
    }
    if (key == "path") {
        next.repo.path = normalize_path_prefix(value);
        return next;
    }
    if (key == "commit.message_template" || key == "commit.update_template") {
        if (util::trim(value).empty()) throw ConfigError(key + " cannot be empty");
        if (key == "commit.message_template") {
            next.commit.message_template = value;
        } else {
            next.commit.update_template = value;
        }
        return next;
    }
    if (key == "commit.prompt") {
        std::string mode = util::to_lower(util::trim(value));
        bool known = false;
        for (const std::string& candidate : prompt_modes()) {
            if (mode == candidate) known = true;
        }
        if (!known) {
            throw ConfigError("commit.prompt must be one of " +
                              util::join(prompt_modes(), ", ") + ", got: " + value);
        }
        next.commit.prompt = mode;
        return next;
    }
    if (key == "commit.author_name" || key == "commit.author_email") {
        if (key == "commit.author_name") {
            next.commit.author_name = util::trim(value);
        } else {
            next.commit.author_email = util::trim(value);
        }
        return next;
    }
    if (key == "cache.problems_ttl_days") {
        int days = 0;
        try {
            size_t used = 0;
            std::string trimmed = util::trim(value);
            days = std::stoi(trimmed, &used);
            if (used != trimmed.size()) throw std::invalid_argument(value);
        } catch (const std::exception&) {
            throw ConfigError("cache.problems_ttl_days must be an integer: " + value);
        }
        if (days < 0) throw ConfigError("cache.problems_ttl_days must be >= 0");
        next.cache.problems_ttl_days = days;
        return next;
    }
    throw ConfigError("Unknown config key: " + key +
                      ". Known keys: " + util::join(settable_keys(), ", "));
}

std::string render_show(const Config& config, bool token_present,
                        const std::string& token_source) {
    std::string status =
        token_present ? "set (source: " + token_source + ")" : "not set";
    return dumps(config) + "\n[token]\nstatus = \"" + status +
           "\"  # value redacted, never stored in config.toml";
}

}  // namespace lcpush::config
