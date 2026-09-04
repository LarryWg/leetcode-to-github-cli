#include "lcpush/ui/picker.hpp"

#include <algorithm>
#include <vector>

#include "lcpush/core/errors.hpp"
#include "lcpush/term/raw_mode.hpp"
#include "lcpush/term/render.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::picker {

namespace {

using term::Key;

constexpr const char* kPrompt = "? Question:  ";  // 13 columns, like the Python VSplit

std::string difficulty_color(const std::string& difficulty) {
    std::string lower = util::to_lower(difficulty);
    if (lower == "easy") return "\033[32m";
    if (lower == "medium") return "\033[33m";
    if (lower == "hard") return "\033[31m";
    return "";
}

struct RowParts {
    std::string left;  // marker + title + padding
    std::string tag;   // [Difficulty] + lock
};

RowParts row_parts(const Question& question, int width, bool selected) {
    std::string tag = "[" + question.difficulty + "]";
    std::string lock = question.paid ? " 🔒" : "";
    std::string left = question.display();
    long room = std::max(width, kMinWidth) -
                static_cast<long>(util::codepoint_count(tag)) -
                static_cast<long>(util::codepoint_count(lock)) - 4;
    long left_len = static_cast<long>(util::codepoint_count(left));
    if (left_len > room) {
        left = util::codepoint_prefix(left, static_cast<size_t>(std::max(room - 1, 1L))) +
               "…";
        left_len = static_cast<long>(util::codepoint_count(left));
    }
    long padding = std::max(1L, room - left_len);
    RowParts parts;
    parts.left = (selected ? "  ❯ " : "    ") + left + std::string(padding, ' ');
    parts.tag = tag + lock;
    return parts;
}

}  // namespace

std::string format_row(const Question& question, int width) {
    RowParts parts = row_parts(question, width, false);
    // The standalone helper has no marker column, matching the Python one.
    return parts.left.substr(4) + parts.tag;
}

Question pick(term::Terminal& terminal, const search::ProblemIndex& index,
              const std::string& initial) {
    if (index.questions.empty()) throw Cancelled("No questions to choose from.");

    // Query buffer, one code point per element so cursor math is direct.
    std::vector<std::string> query;
    std::string rest = initial;
    while (!rest.empty()) {
        std::string first = util::codepoint_prefix(rest, 1);
        if (first.empty()) break;
        query.push_back(first);
        rest = rest.substr(first.size());
    }
    size_t query_cursor = query.size();

    auto query_text = [&query] {
        std::string out;
        for (const std::string& cp : query) out += cp;
        return out;
    };

    std::vector<Question> matches =
        search::search(index, query_text(), kResultLimit);
    size_t cursor = 0;

    term::RawMode raw(terminal.interactive);
    term::Frame frame(terminal.out);
    bool color = terminal.interactive;

    auto refresh = [&] {
        matches = search::search(index, query_text(), kResultLimit);
        cursor = 0;
    };

    while (true) {
        int width = terminal.size().first - 6;
        std::string content;
        if (color) {
            content = "\033[1m" + std::string(kPrompt) + "\033[0m" + query_text();
        } else {
            content = std::string(kPrompt) + query_text();
        }
        if (matches.empty()) {
            content += "\n";
            content += color ? "\033[90m    no matches\033[0m" : "    no matches";
        } else {
            for (size_t i = 0; i < matches.size(); ++i) {
                RowParts parts = row_parts(matches[i], width, i == cursor);
                content += "\n";
                if (color && i == cursor) {
                    content += "\033[7m" + parts.left + "\033[27m";
                } else {
                    content += parts.left;
                }
                std::string tint = color ? difficulty_color(matches[i].difficulty) : "";
                if (!tint.empty()) {
                    content += tint + parts.tag + "\033[0m";
                } else {
                    content += parts.tag;
                }
            }
            content += "\n";
            content += color ? "\033[90m    ↑/↓ to move, Enter to select\033[0m"
                             : "    ↑/↓ to move, Enter to select";
        }
        frame.render(content, 0,
                     static_cast<int>(util::codepoint_count(kPrompt) + query_cursor));

        Key key = terminal.keys.next();
        switch (key.type) {
            case Key::Type::Enter:
                if (!matches.empty()) {
                    Question chosen = matches[cursor];
                    frame.clear();
                    return chosen;
                }
                break;
            case Key::Type::Esc:
            case Key::Type::CtrlC:
            case Key::Type::Eof:
                frame.clear();
                throw Cancelled();
            case Key::Type::Up:
            case Key::Type::CtrlP:
                if (!matches.empty()) {
                    cursor = (cursor + matches.size() - 1) % matches.size();
                }
                break;
            case Key::Type::Down:
            case Key::Type::CtrlN:
                if (!matches.empty()) cursor = (cursor + 1) % matches.size();
                break;
            case Key::Type::Char:
                query.insert(query.begin() + static_cast<long>(query_cursor), key.text);
                ++query_cursor;
                refresh();
                break;
            case Key::Type::Backspace:
                if (query_cursor > 0) {
                    query.erase(query.begin() + static_cast<long>(query_cursor) - 1);
                    --query_cursor;
                    refresh();
                }
                break;
            case Key::Type::Left:
                if (query_cursor > 0) --query_cursor;
                break;
            case Key::Type::Right:
                if (query_cursor < query.size()) ++query_cursor;
                break;
            case Key::Type::Home:
            case Key::Type::CtrlA:
                query_cursor = 0;
                break;
            case Key::Type::End:
            case Key::Type::CtrlE:
                query_cursor = query.size();
                break;
            case Key::Type::CtrlU:
                query.erase(query.begin(), query.begin() + static_cast<long>(query_cursor));
                query_cursor = 0;
                refresh();
                break;
            default:
                break;
        }
    }
}

}  // namespace lcpush::picker
