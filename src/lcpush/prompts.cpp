#include "lcpush/prompts.hpp"

#include <vector>

#include "lcpush/errors.hpp"
#include "lcpush/term/raw_mode.hpp"
#include "lcpush/term/render.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::prompts {

namespace {

using term::Key;

constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kDim = "\033[2m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kCyan = "\033[36m";

std::string styled(bool on, const char* code, const std::string& text) {
    return on ? std::string(code) + text + kReset : text;
}

// The line-editor core shared by text, password, and edit_line. The buffer
// is a code point sequence so cursor math matches what the terminal shows.
class LineEditor {
  public:
    LineEditor(term::Terminal& terminal, std::string prefix, const std::string& initial,
               bool mask)
        : terminal_(terminal), frame_(terminal.out), prefix_(std::move(prefix)),
          mask_(mask) {
        for (char32_t cp : util::to_u32(initial)) {
            std::u32string one(1, cp);
            buffer_.push_back(to_utf8(one));
        }
        cursor_ = buffer_.size();
    }

    std::string run() {
        term::RawMode raw(terminal_.interactive);
        while (true) {
            paint();
            Key key = terminal_.keys.next();
            switch (key.type) {
                case Key::Type::Enter: {
                    std::string result = value();
                    frame_.finish(prefix_ + display_value());
                    return result;
                }
                case Key::Type::CtrlC:
                case Key::Type::CtrlD:
                case Key::Type::Eof:
                    frame_.clear();
                    throw Cancelled();
                case Key::Type::Char:
                    buffer_.insert(buffer_.begin() + static_cast<long>(cursor_), key.text);
                    ++cursor_;
                    break;
                case Key::Type::Backspace:
                    if (cursor_ > 0) {
                        buffer_.erase(buffer_.begin() + static_cast<long>(cursor_) - 1);
                        --cursor_;
                    }
                    break;
                case Key::Type::Left:
                    if (cursor_ > 0) --cursor_;
                    break;
                case Key::Type::Right:
                    if (cursor_ < buffer_.size()) ++cursor_;
                    break;
                case Key::Type::Home:
                case Key::Type::CtrlA:
                    cursor_ = 0;
                    break;
                case Key::Type::End:
                case Key::Type::CtrlE:
                    cursor_ = buffer_.size();
                    break;
                case Key::Type::CtrlU:
                    // Kill from the cursor back to the start of the line.
                    buffer_.erase(buffer_.begin(),
                                  buffer_.begin() + static_cast<long>(cursor_));
                    cursor_ = 0;
                    break;
                default:
                    break;
            }
        }
    }

  private:
    static std::string to_utf8(const std::u32string& text) {
        std::string out;
        for (char32_t cp : text) {
            if (cp < 0x80) {
                out.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else if (cp < 0x10000) {
                out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else {
                out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            }
        }
        return out;
    }

    std::string value() const {
        std::string out;
        for (const std::string& cp : buffer_) out += cp;
        return out;
    }

    std::string display_value() const {
        if (!mask_) return value();
        return std::string(buffer_.size(), '*');
    }

    void paint() {
        int col = static_cast<int>(util::codepoint_count(prefix_) + cursor_);
        frame_.render(prefix_ + display_value(), 0, col);
    }

    term::Terminal& terminal_;
    term::Frame frame_;
    std::string prefix_;
    bool mask_;
    std::vector<std::string> buffer_;  // one code point per element
    size_t cursor_ = 0;
};

}  // namespace

std::string select(term::Terminal& terminal, const std::string& message,
                   const std::vector<Choice>& choices,
                   const std::optional<std::string>& default_value) {
    if (choices.empty()) throw Cancelled();
    size_t cursor = 0;
    if (default_value) {
        for (size_t i = 0; i < choices.size(); ++i) {
            if (choices[i].value == *default_value) cursor = i;
        }
    }

    term::RawMode raw(terminal.interactive);
    term::Frame frame(terminal.out);
    bool color = terminal.interactive;
    while (true) {
        std::string content =
            styled(color, kBold, message) + "  " +
            styled(color, kDim, "↑/↓ to change, Enter to accept");
        for (size_t i = 0; i < choices.size(); ++i) {
            content += "\n";
            if (i == cursor) {
                content += styled(color, kCyan, "  ❯ " + choices[i].title);
            } else {
                content += "    " + choices[i].title;
            }
        }
        frame.render(content);

        Key key = terminal.keys.next();
        switch (key.type) {
            case Key::Type::Enter:
                frame.finish(message + " " +
                             styled(color, kGreen, choices[cursor].title));
                return choices[cursor].value;
            case Key::Type::Up:
            case Key::Type::CtrlP:
                cursor = (cursor + choices.size() - 1) % choices.size();
                break;
            case Key::Type::Down:
            case Key::Type::CtrlN:
                cursor = (cursor + 1) % choices.size();
                break;
            case Key::Type::CtrlC:
            case Key::Type::CtrlD:
            case Key::Type::Eof:
                frame.clear();
                throw Cancelled();
            default:
                break;
        }
    }
}

bool confirm(term::Terminal& terminal, const std::string& message, bool default_value) {
    term::RawMode raw(terminal.interactive);
    term::Frame frame(terminal.out);
    bool color = terminal.interactive;
    const std::string hint = default_value ? "(Y/n)" : "(y/N)";
    while (true) {
        frame.render(styled(color, kBold, message) + " " + styled(color, kDim, hint) + " ");
        Key key = terminal.keys.next();
        std::optional<bool> answer;
        if (key.type == Key::Type::Enter) answer = default_value;
        if (key.type == Key::Type::Char && (key.text == "y" || key.text == "Y")) {
            answer = true;
        }
        if (key.type == Key::Type::Char && (key.text == "n" || key.text == "N")) {
            answer = false;
        }
        if (key.type == Key::Type::CtrlC || key.type == Key::Type::CtrlD ||
            key.type == Key::Type::Eof) {
            frame.clear();
            throw Cancelled();
        }
        if (answer.has_value()) {
            frame.finish(message + " " + styled(color, kGreen, *answer ? "Yes" : "No"));
            return *answer;
        }
    }
}

std::string text(term::Terminal& terminal, const std::string& message,
                 const std::string& default_value) {
    return LineEditor(terminal, message + " ", default_value, false).run();
}

std::string password(term::Terminal& terminal, const std::string& message) {
    return LineEditor(terminal, message + " ", "", true).run();
}

std::string edit_line(term::Terminal& terminal, const std::string& message,
                      const std::string& default_value) {
    return LineEditor(terminal, message, default_value, false).run();
}

std::string read_key(term::Terminal& terminal,
                     const std::map<std::string, std::string>& allowed) {
    term::RawMode raw(terminal.interactive);
    while (true) {
        Key key = terminal.keys.next();
        if (key.type == Key::Type::CtrlC || key.type == Key::Type::CtrlD ||
            key.type == Key::Type::Eof) {
            throw Cancelled();
        }
        std::string name;
        if (key.type == Key::Type::Enter) {
            name = "enter";
        } else if (key.type == Key::Type::Esc) {
            name = "escape";
        } else if (key.type == Key::Type::Char) {
            name = key.text;
        } else {
            continue;
        }
        auto hit = allowed.find(name);
        if (hit != allowed.end()) return hit->second;
    }
}

}  // namespace lcpush::prompts
