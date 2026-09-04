#include "lcpush/domain/solution.hpp"

#include <rapidfuzz/fuzz.hpp>

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

#include "lcpush/util/strings.hpp"

namespace lcpush::solution {

namespace {

std::regex rx(const char* pattern) {
    return std::regex(pattern, std::regex::ECMAScript | std::regex::multiline);
}

const std::vector<std::regex>& entry_point_patterns() {
    static const std::vector<std::regex> patterns = {
        rx(R"(^\s*def\s+(\w+)\s*\()"),                 // Python / Ruby
        rx(R"(^\s*(?:pub\s+)?fn\s+(\w+)\s*[(<])"),     // Rust / Swift-ish
        rx(R"(^\s*func\s+(\w+)\s*[(<])"),              // Go / Swift
        rx(R"(^\s*fun\s+(\w+)\s*\()"),                 // Kotlin
        rx(R"(^\s*(?:public|private|protected|static|final|\s)*[\w<>\[\],\s*&:]+?\s+(\w+)\s*\([^;]*\)\s*\{)"),
        rx(R"(\b(?:var|const|let)\s+(\w+)\s*=\s*function\s*\()"),
        rx(R"(^\s*function\s+(\w+)\s*\()"),
    };
    return patterns;
}

const std::set<std::string>& skip_entry_names() {
    static const std::set<std::string> names = {
        "if",  "for",   "while",  "switch",   "catch",    "return",
        "main", "new",  "class",  "struct",   "__init__", "Solution",
    };
    return names;
}

// Keep only ASCII lowercase letters and digits of the lowercased input.
std::string flatten(const std::string& text) {
    std::string out;
    for (char c : util::to_lower(text)) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out.push_back(c);
    }
    return out;
}

std::vector<std::string> slug_variants(const std::string& slug) {
    std::vector<std::string> parts;
    for (const std::string& part : util::split(slug, '-')) {
        if (!part.empty()) parts.push_back(part);
    }
    if (parts.empty()) return {};
    std::string camel = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        std::string piece = util::to_lower(parts[i]);
        piece[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(piece[0])));
        camel += piece;
    }
    return {camel, util::join(parts, "_"), util::join(parts, "")};
}

}  // namespace

std::string normalize(const std::string& text) {
    std::string unified = util::replace_all(text, "\r\n", "\n");
    unified = util::replace_all(std::move(unified), "\r", "\n");
    std::vector<std::string> lines;
    for (const std::string& line : util::split(unified, '\n')) {
        lines.push_back(util::rstrip(line));
    }
    std::string body = util::rstrip(util::join(lines, "\n"), '\n');
    return body.empty() ? "" : body + "\n";
}

int line_count(const std::string& text) {
    std::string stripped = util::rstrip(text, '\n');
    if (stripped.empty()) return 0;
    return static_cast<int>(util::split(stripped, '\n').size());
}

size_t byte_size(const std::string& text) { return text.size(); }

std::optional<std::string> reject_reason(const std::string& text) {
    if (util::trim(text).empty()) return "Content is empty.";
    if (byte_size(text) > kMaxBytes) {
        return "Content is " + std::to_string(byte_size(text) / 1024) +
               "KB, over the 1MB limit.";
    }
    return std::nullopt;
}

std::vector<std::string> entry_point_names(const std::string& text) {
    std::vector<std::string> found;
    for (const std::regex& pattern : entry_point_patterns()) {
        for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
             it != std::sregex_iterator(); ++it) {
            std::string name = (*it)[1].str();
            if (skip_entry_names().count(name)) continue;
            if (std::find(found.begin(), found.end(), name) != found.end()) continue;
            found.push_back(name);
        }
    }
    return found;
}

bool brackets_balanced(const std::string& text) {
    std::vector<char> stack;
    size_t index = 0;
    const size_t length = text.size();
    auto closes = [](char c) -> char {
        switch (c) {
            case ')': return '(';
            case ']': return '[';
            case '}': return '{';
            default: return 0;
        }
    };
    while (index < length) {
        char c = text[index];
        if (c == '"' || c == '\'') {
            char quote = c;
            ++index;
            while (index < length) {
                if (text[index] == '\\') {
                    index += 2;
                    continue;
                }
                if (text[index] == quote || text[index] == '\n') break;
                ++index;
            }
            ++index;
            continue;
        }
        if (c == '#' || (c == '/' && text.compare(index, 2, "//") == 0)) {
            size_t newline = text.find('\n', index);
            index = newline == std::string::npos ? length : newline;
            continue;
        }
        if (text.compare(index, 2, "/*") == 0) {
            size_t end = text.find("*/", index + 2);
            index = end == std::string::npos ? length : end + 2;
            continue;
        }
        if (c == '(' || c == '[' || c == '{') {
            stack.push_back(c);
        } else if (char open = closes(c)) {
            if (stack.empty() || stack.back() != open) return false;
            stack.pop_back();
        }
        ++index;
    }
    return stack.empty();
}

bool matches_slug(const std::string& name, const std::string& slug) {
    if (slug.empty()) return true;
    std::string flat_name = flatten(name);
    if (flat_name.empty()) return true;
    for (const std::string& variant : slug_variants(slug)) {
        std::string flat_variant = flatten(variant);
        if (flat_variant.empty()) continue;
        if (flat_name == flat_variant) return true;
        if (flat_variant.find(flat_name) != std::string::npos ||
            flat_name.find(flat_variant) != std::string::npos) {
            return true;
        }
        if (rapidfuzz::fuzz::ratio(flat_name, flat_variant) >= 70.0) return true;
    }
    return false;
}

std::vector<std::string> soft_warnings(const std::string& text,
                                       const detect::Detection* detection,
                                       const std::string& slug,
                                       const std::string& title) {
    std::vector<std::string> messages;

    if (detection != nullptr && !detection->confident()) {
        messages.push_back("⚠ Could not identify a programming language.");
    }

    if (!slug.empty()) {
        auto names = entry_point_names(text);
        bool any_match = std::any_of(names.begin(), names.end(), [&](const std::string& name) {
            return matches_slug(name, slug);
        });
        if (!names.empty() && !any_match) {
            const std::string& label = title.empty() ? slug : title;
            messages.push_back("⚠ This defines " + names[0] + ", but you selected \"" +
                               label + "\". Wrong question?");
        }
    }

    if (text.find("<<<<<<<") != std::string::npos ||
        text.find(">>>>>>>") != std::string::npos ||
        text.find("TODO") != std::string::npos) {
        messages.push_back("⚠ Content contains conflict markers or TODOs.");
    }

    if (!brackets_balanced(text)) {
        messages.push_back("⚠ Brackets are unbalanced — the paste may be truncated.");
    }

    return messages;
}

}  // namespace lcpush::solution
