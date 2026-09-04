#include "lcpush/ui/console.hpp"

#include <unistd.h>

#include <cstdio>
#include <cstdlib>

#include "lcpush/domain/solution.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::ui {

namespace {

bool env_set(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0';
}

void print_line(FILE* stream, const std::string& text) {
    std::fputs(text.c_str(), stream);
    std::fputc('\n', stream);
    std::fflush(stream);
}

FILE* g_out = nullptr;
FILE* g_err = nullptr;

}  // namespace

void set_streams(std::FILE* out, std::FILE* err) {
    g_out = out;
    g_err = err;
}

std::FILE* out_stream() { return g_out != nullptr ? g_out : stdout; }

std::FILE* err_stream() { return g_err != nullptr ? g_err : stderr; }

bool color_enabled() {
    if (env_set("NO_COLOR")) return false;
    if (env_set("LCPUSH_FORCE_COLOR")) return true;
    return ::isatty(::fileno(stdout)) != 0;
}

std::string paint(const std::string& text, const char* code) {
    if (!color_enabled()) return text;
    return std::string(code) + text + kReset;
}

void success(const std::string& message) { print_line(out_stream(), paint("✓ " + message, kGreen)); }

void info(const std::string& message) { print_line(out_stream(), message); }

void dim(const std::string& message) { print_line(out_stream(), paint(message, kDim)); }

void warn(const std::string& message) { print_line(err_stream(), paint(message, kYellow)); }

void error(const std::string& message) { print_line(err_stream(), paint("✗ " + message, kRed)); }

void arrow(const std::string& message) { print_line(out_stream(), paint("→ " + message, kCyan)); }

std::vector<std::string> preview_lines(const std::string& text) {
    std::vector<std::string> lines = util::split(util::rstrip(text, '\n'), '\n');
    if (lines.size() <= static_cast<size_t>(kHeadLines + kTailLines + 1)) return lines;
    size_t hidden = lines.size() - kHeadLines - kTailLines;
    std::vector<std::string> out(lines.begin(), lines.begin() + kHeadLines);
    out.push_back(" … " + std::to_string(hidden) + " lines hidden …");
    out.insert(out.end(), lines.end() - kTailLines, lines.end());
    return out;
}

std::string render_preview(const std::string& source_label, const std::string& text,
                           const std::string& language_label) {
    std::string header = "┌ " + source_label + " — " +
                         std::to_string(solution::line_count(text)) + " lines, " +
                         std::to_string(solution::byte_size(text)) + " bytes, detected " +
                         language_label;
    std::vector<std::string> rows = {"  " + header};
    for (const std::string& line : preview_lines(text)) {
        rows.push_back("  │ " + line);
    }
    return util::join(rows, "\n");
}

std::string render_push_panel(const PushPanel& panel) {
    std::string title =
        panel.updating ? "Ready to push (overwrites existing file)" : "Ready to push";
    size_t break_at = panel.message.find('\n');
    std::string subject = panel.message.substr(0, break_at);
    std::string body =
        break_at == std::string::npos ? "" : panel.message.substr(break_at + 1);

    std::vector<std::string> rows = {
        "  ┌ " + title,
        "  │ File     " + panel.filename + "  (" + std::to_string(panel.lines) + " lines)",
        "  │ Repo     " + panel.repo + "  (" + panel.branch + ")",
        "  │ Message  " + subject,
    };
    if (!body.empty()) {
        for (const std::string& extra : util::split(body, '\n')) {
            rows.push_back("  │          " + extra);
        }
    }
    if (panel.prompt_mode == "never") {
        rows.push_back("  └ ? [Enter] push   [n] cancel");
    } else {
        rows.push_back(
            "  └ ? [Enter] push   [m] edit message   [M] edit in $EDITOR   [n] cancel");
    }
    return util::join(rows, "\n");
}

}  // namespace lcpush::ui
