// Clipboard reading across platforms. A missing tool or an empty clipboard is
// not an error: the caller simply drops the Clipboard option from the menu.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lcpush::clipboard {

inline constexpr int kTimeoutSeconds = 5;

#if defined(__APPLE__)
inline constexpr const char* kNativePlatform = "darwin";
#elif defined(_WIN32)
inline constexpr const char* kNativePlatform = "win32";
#else
inline constexpr const char* kNativePlatform = "linux";
#endif

// The paste command for a platform, or nullopt if nothing is available.
// The platform parameter exists for tests; production uses the default.
std::optional<std::vector<std::string>> clipboard_command(
    std::string_view platform = kNativePlatform);

bool available();

// Clipboard contents, or nullopt when unreadable, empty, or whitespace-only.
std::optional<std::string> read();

}  // namespace lcpush::clipboard
