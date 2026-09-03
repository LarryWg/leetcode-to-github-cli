#include "lcpush/clipboard.hpp"

#include <cstdlib>

#include "lcpush/util/strings.hpp"
#include "lcpush/util/subprocess.hpp"

namespace lcpush::clipboard {

namespace {

bool has_tool(const std::string& name) {
    return util::subprocess().which(name).has_value();
}

}  // namespace

std::optional<std::vector<std::string>> clipboard_command(std::string_view platform) {
    if (platform == "darwin") {
        if (has_tool("pbpaste")) return std::vector<std::string>{"pbpaste"};
        return std::nullopt;
    }
    if (platform == "win32") {
        if (has_tool("powershell")) {
            return std::vector<std::string>{"powershell", "-NoProfile", "-Command",
                                            "Get-Clipboard"};
        }
        return std::nullopt;
    }
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if (wayland != nullptr && *wayland != '\0' && has_tool("wl-paste")) {
        return std::vector<std::string>{"wl-paste", "--no-newline"};
    }
    if (has_tool("xclip")) {
        return std::vector<std::string>{"xclip", "-selection", "clipboard", "-o"};
    }
    if (has_tool("xsel")) return std::vector<std::string>{"xsel", "-b"};
    if (has_tool("wl-paste")) return std::vector<std::string>{"wl-paste", "--no-newline"};
    return std::nullopt;
}

bool available() { return clipboard_command().has_value(); }

std::optional<std::string> read() {
    auto command = clipboard_command();
    if (!command) return std::nullopt;
    util::RunResult result = util::subprocess().run_capture(*command, kTimeoutSeconds);
    if (!result.ok || result.exit_code != 0) return std::nullopt;
    if (util::trim(result.out).empty()) return std::nullopt;
    return result.out;
}

}  // namespace lcpush::clipboard
