#include "lcpush/domain/plausibility.hpp"

#include <regex>

#include "lcpush/domain/detect.hpp"
#include "lcpush/domain/solution.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::plausibility {

namespace {

const std::regex& entry_shapes() {
    static const std::regex value(
        R"(class\s+Solution|impl\s+Solution|object\s+Solution|public\s+class\s+Solution)"
        R"(|var\s+\w+\s*=\s*function\s*\(|func\s+\w+\s*\(|def\s+\w+\s*\(|fun\s+\w+\s*\()",
        std::regex::ECMAScript | std::regex::multiline);
    return value;
}

const std::regex& bare_url() {
    static const std::regex value(R"(^\s*(https?://|www\.)\S+\s*$)",
                                  std::regex::ECMAScript | std::regex::icase);
    return value;
}

const std::regex& bare_email() {
    static const std::regex value(R"(^\s*[\w.+-]+@[\w-]+\.[\w.]+\s*$)");
    return value;
}

const std::regex& bare_path() {
    static const std::regex value(R"(^\s*(~|\.{0,2}/|[A-Za-z]:\\)[\w./\\ -]*\s*$)");
    return value;
}

const std::regex& word() {
    static const std::regex value(R"([A-Za-z']+)");
    return value;
}

const std::regex& symbol() {
    static const std::regex value(R"([{}()\[\];:=<>+*/&|%!#$@\\])");
    return value;
}

size_t count_matches(const std::string& text, const std::regex& pattern) {
    size_t count = 0;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
         it != std::sregex_iterator(); ++it) {
        ++count;
    }
    return count;
}

// High word-to-symbol ratio: the signature of chat messages and articles.
bool prose_heavy(const std::string& text) {
    size_t words = count_matches(text, word());
    size_t symbols = count_matches(text, symbol());
    if (words < 12) return false;
    if (symbols == 0) return true;
    return static_cast<double>(words) / static_cast<double>(symbols) > 12.0;
}

// Non-empty logical lines of the trimmed text, like Python str.splitlines
// on the inputs this sees (LF-normalized clipboard text).
std::vector<std::string> split_lines(const std::string& text) {
    return util::split(text, '\n');
}

}  // namespace

Plausibility assess(const std::optional<std::string>& text) {
    if (!text || util::trim(*text).empty()) {
        return Plausibility{false, 0, {"clipboard is empty"}};
    }

    int points = 0;
    std::vector<std::string> reasons;
    std::string stripped = util::trim(*text);
    std::vector<std::string> lines = split_lines(stripped);

    if (detect::detect(*text).confident()) {
        points += 3;
        reasons.push_back("language detected");
    }
    if (std::regex_search(*text, entry_shapes())) {
        points += 3;
        reasons.push_back("has a solution entry point");
    }
    if (lines.size() >= 2) {
        points += 1;
        reasons.push_back("multi-line");
    }
    bool has_indent = false;
    for (const std::string& line : lines) {
        if (!util::trim(line).empty() && (line.rfind(' ', 0) == 0 || line.rfind('\t', 0) == 0)) {
            has_indent = true;
            break;
        }
    }
    if (std::regex_search(*text, symbol()) && has_indent) {
        points += 1;
        reasons.push_back("indented code-like punctuation");
    }

    if (lines.size() == 1 &&
        (std::regex_match(stripped, bare_url()) || std::regex_match(stripped, bare_email()) ||
         std::regex_match(stripped, bare_path()))) {
        points -= 5;
        reasons.push_back("looks like a bare URL, email, or path");
    }
    if (lines.size() == 1 && util::codepoint_count(stripped) < 40) {
        points -= 3;
        reasons.push_back("single short line");
    }
    if (prose_heavy(*text)) {
        points -= 3;
        reasons.push_back("reads as prose");
    }
    if (solution::byte_size(*text) > solution::kLargeClipboardBytes) {
        points -= 3;
        reasons.push_back("over 200KB");
    }

    return Plausibility{points >= kPlausibleThreshold, points, std::move(reasons)};
}

}  // namespace lcpush::plausibility
