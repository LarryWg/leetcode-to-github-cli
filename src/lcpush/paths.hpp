// XDG-correct config and cache locations, with LCPUSH_* overrides for tests.
#pragma once

#include <filesystem>
#include <string>

namespace lcpush::paths {

// Directory holding config.toml and the token fallback file.
std::filesystem::path config_dir();

// Directory holding problems.json.
std::filesystem::path cache_dir();

std::filesystem::path config_file();
std::filesystem::path token_file();
std::filesystem::path problems_cache_file();

// Write text with mode 0600, creating parent directories as needed.
void write_private(const std::filesystem::path& path, const std::string& text);

}  // namespace lcpush::paths
