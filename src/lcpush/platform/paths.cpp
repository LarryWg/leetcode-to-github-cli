#include "lcpush/platform/paths.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <system_error>

#include "lcpush/core/errors.hpp"

namespace lcpush::paths {

namespace {

constexpr const char* kAppName = "lcpush";

std::filesystem::path home_dir() {
    const char* home = std::getenv("HOME");
    return home ? std::filesystem::path(home) : std::filesystem::path("/");
}

// Expand a leading ~ like Python's Path.expanduser.
std::filesystem::path expand_user(const std::string& value) {
    if (value == "~") return home_dir();
    if (value.rfind("~/", 0) == 0) return home_dir() / value.substr(2);
    return std::filesystem::path(value);
}

std::filesystem::path xdg_dir(const char* override_var, const char* xdg_var,
                              const char* fallback_leaf) {
    const char* override_dir = std::getenv(override_var);
    if (override_dir && *override_dir) return expand_user(override_dir);
    const char* xdg = std::getenv(xdg_var);
    if (xdg && *xdg) return expand_user(xdg) / kAppName;
    return home_dir() / fallback_leaf / kAppName;
}

}  // namespace

std::filesystem::path config_dir() {
    return xdg_dir("LCPUSH_CONFIG_DIR", "XDG_CONFIG_HOME", ".config");
}

std::filesystem::path cache_dir() {
    return xdg_dir("LCPUSH_CACHE_DIR", "XDG_CACHE_HOME", ".cache");
}

std::filesystem::path config_file() { return config_dir() / "config.toml"; }

std::filesystem::path token_file() { return config_dir() / "token"; }

std::filesystem::path problems_cache_file() { return cache_dir() / "problems.json"; }

void write_private(const std::filesystem::path& path, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    // Create with restrictive permissions before any bytes hit the disk.
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        throw ConfigError("Could not write " + path.string() + ": " + std::strerror(errno));
    }
    size_t written = 0;
    while (written < text.size()) {
        ssize_t n = ::write(fd, text.data() + written, text.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            int saved = errno;
            ::close(fd);
            throw ConfigError("Could not write " + path.string() + ": " + std::strerror(saved));
        }
        written += static_cast<size_t>(n);
    }
    ::close(fd);
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
}

}  // namespace lcpush::paths
