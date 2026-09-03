// $EDITOR / stdin solution sources.
#pragma once

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace lcpush::editor {

inline constexpr const char* kSolutionHeader =
    "# Paste your solution below. Save and close to continue.\n";
inline constexpr const char* kStdinSentinel = "EOF";

// $VISUAL -> $EDITOR -> vi, split shell-style.
std::vector<std::string> editor_command();

// Remove the instruction header we injected, wherever the user left it.
std::string strip_header(const std::string& text, const std::string& header);

struct EditorOptions {
    std::string initial;
    std::string header = kSolutionHeader;
    std::string suffix = ".txt";
};

// Open an editor on a temp file. Returns the content, or nullopt when the
// editor failed to spawn, the file is unchanged, or the body is empty.
std::optional<std::string> open_editor(const EditorOptions& options = {});

// Read until end of stream or a lone EOF sentinel line.
std::string read_stdin(std::istream& stream);

}  // namespace lcpush::editor
