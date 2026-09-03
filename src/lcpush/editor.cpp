#include "lcpush/editor.hpp"

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include "lcpush/util/strings.hpp"
#include "lcpush/util/subprocess.hpp"

namespace lcpush::editor {

namespace {

// Minimal POSIX shlex.split. An unclosed quote falls back to the raw string,
// matching the Python behavior of catching ValueError.
std::optional<std::vector<std::string>> shell_split(const std::string& raw) {
    std::vector<std::string> parts;
    std::string current;
    bool in_token = false;
    size_t i = 0;
    while (i < raw.size()) {
        char c = raw[i];
        if (c == ' ' || c == '\t' || c == '\n') {
            if (in_token) {
                parts.push_back(current);
                current.clear();
                in_token = false;
            }
            ++i;
            continue;
        }
        in_token = true;
        if (c == '\'') {
            size_t end = raw.find('\'', i + 1);
            if (end == std::string::npos) return std::nullopt;
            current.append(raw, i + 1, end - i - 1);
            i = end + 1;
            continue;
        }
        if (c == '"') {
            ++i;
            bool closed = false;
            while (i < raw.size()) {
                if (raw[i] == '\\' && i + 1 < raw.size() &&
                    (raw[i + 1] == '"' || raw[i + 1] == '\\')) {
                    current.push_back(raw[i + 1]);
                    i += 2;
                    continue;
                }
                if (raw[i] == '"') {
                    closed = true;
                    ++i;
                    break;
                }
                current.push_back(raw[i]);
                ++i;
            }
            if (!closed) return std::nullopt;
            continue;
        }
        if (c == '\\' && i + 1 < raw.size()) {
            current.push_back(raw[i + 1]);
            i += 2;
            continue;
        }
        current.push_back(c);
        ++i;
    }
    if (in_token) parts.push_back(current);
    return parts;
}

}  // namespace

std::vector<std::string> editor_command() {
    const char* raw = std::getenv("VISUAL");
    if (raw == nullptr || *raw == '\0') raw = std::getenv("EDITOR");
    if (raw == nullptr || *raw == '\0') raw = "vi";
    auto parts = shell_split(raw);
    if (!parts) return {std::string(raw)};
    if (parts->empty()) return {"vi"};
    return *parts;
}

std::string strip_header(const std::string& text, const std::string& header) {
    std::string unified = util::replace_all(text, "\r\n", "\n");
    std::set<std::string> header_lines;
    for (const std::string& line : util::split(header, '\n')) {
        std::string stripped = util::trim(line);
        if (!stripped.empty()) header_lines.insert(stripped);
    }
    std::vector<std::string> kept;
    for (const std::string& line : util::split(unified, '\n')) {
        if (header_lines.count(util::trim(line))) continue;
        kept.push_back(line);
    }
    return util::join(kept, "\n");
}

std::optional<std::string> open_editor(const EditorOptions& options) {
    std::string seed = options.header + options.initial;
    std::string tmpl =
        (std::filesystem::temp_directory_path() / "lcpush-XXXXXX").string() + options.suffix;
    std::vector<char> path_buffer(tmpl.begin(), tmpl.end());
    path_buffer.push_back('\0');
    int fd = ::mkstemps(path_buffer.data(), static_cast<int>(options.suffix.size()));
    if (fd < 0) return std::nullopt;
    std::string path(path_buffer.data());

    struct Cleanup {
        std::string path;
        ~Cleanup() { ::unlink(path.c_str()); }
    } cleanup{path};

    size_t written = 0;
    while (written < seed.size()) {
        ssize_t n = ::write(fd, seed.data() + written, seed.size() - written);
        if (n < 0) {
            ::close(fd);
            return std::nullopt;
        }
        written += static_cast<size_t>(n);
    }
    ::close(fd);

    std::vector<std::string> command = editor_command();
    command.push_back(path);
    if (!util::subprocess().run_interactive(command)) return std::nullopt;

    std::ifstream in(path);
    if (!in.good()) return std::nullopt;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    std::string result = buffer.str();

    if (result == seed) return std::nullopt;
    std::string body =
        options.header.empty() ? result : strip_header(result, options.header);
    if (util::trim(body).empty()) return std::nullopt;
    return body;
}

std::string read_stdin(std::istream& stream) {
    std::string out;
    std::string line;
    while (std::getline(stream, line)) {
        bool had_newline = !stream.eof();
        if (util::rstrip(line, '\r') == kStdinSentinel) break;
        out += line;
        if (had_newline) out += "\n";
    }
    return out;
}

}  // namespace lcpush::editor
