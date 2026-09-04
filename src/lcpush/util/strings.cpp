#include "lcpush/util/strings.hpp"

#include <algorithm>
#include <cctype>

namespace lcpush::util {

namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

}  // namespace

std::string trim(std::string_view text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && is_space(text[begin])) ++begin;
    while (end > begin && is_space(text[end - 1])) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string rstrip(std::string_view text) {
    size_t end = text.size();
    while (end > 0 && is_space(text[end - 1])) --end;
    return std::string(text.substr(0, end));
}

std::string rstrip(std::string_view text, char what) {
    size_t end = text.size();
    while (end > 0 && text[end - 1] == what) --end;
    return std::string(text.substr(0, end));
}

std::string strip_chars(std::string_view text, std::string_view chars) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && chars.find(text[begin]) != std::string_view::npos) ++begin;
    while (end > begin && chars.find(text[end - 1]) != std::string_view::npos) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string to_lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool starts_with(std::string_view text, std::string_view prefix) {
    return text.substr(0, prefix.size()) == prefix;
}

bool ends_with(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::string replace_all(std::string text, std::string_view from, std::string_view to) {
    if (from.empty()) return text;
    std::string out;
    out.reserve(text.size());
    size_t pos = 0;
    while (true) {
        size_t hit = text.find(from, pos);
        if (hit == std::string::npos) {
            out.append(text, pos, std::string::npos);
            return out;
        }
        out.append(text, pos, hit - pos);
        out.append(to);
        pos = hit + from.size();
    }
}

std::vector<std::string> split(std::string_view text, char sep) {
    std::vector<std::string> parts;
    size_t pos = 0;
    while (true) {
        size_t hit = text.find(sep, pos);
        if (hit == std::string_view::npos) {
            parts.emplace_back(text.substr(pos));
            return parts;
        }
        parts.emplace_back(text.substr(pos, hit - pos));
        pos = hit + 1;
    }
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) out.append(sep);
        out.append(parts[i]);
    }
    return out;
}

std::string zfill(std::string_view text, size_t width) {
    if (text.size() >= width) return std::string(text);
    return std::string(width - text.size(), '0') + std::string(text);
}

bool is_all_digits(std::string_view text) {
    if (text.empty()) return false;
    return std::all_of(text.begin(), text.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::u32string to_u32(std::string_view text) {
    std::u32string out;
    out.reserve(text.size());
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        unsigned char lead = static_cast<unsigned char>(text[i]);
        char32_t cp = 0;
        size_t extra = 0;
        if (lead < 0x80) {
            cp = lead;
        } else if ((lead >> 5) == 0x6) {
            cp = lead & 0x1f;
            extra = 1;
        } else if ((lead >> 4) == 0xe) {
            cp = lead & 0x0f;
            extra = 2;
        } else if ((lead >> 3) == 0x1e) {
            cp = lead & 0x07;
            extra = 3;
        } else {
            out.push_back(0xfffd);
            ++i;
            continue;
        }
        if (i + extra >= n) {
            out.push_back(0xfffd);
            ++i;
            continue;
        }
        bool valid = true;
        for (size_t k = 1; k <= extra; ++k) {
            unsigned char cont = static_cast<unsigned char>(text[i + k]);
            if ((cont >> 6) != 0x2) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (cont & 0x3f);
        }
        if (!valid) {
            out.push_back(0xfffd);
            ++i;
            continue;
        }
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

size_t codepoint_count(std::string_view text) {
    size_t count = 0;
    for (char c : text) {
        if ((static_cast<unsigned char>(c) >> 6) != 0x2) ++count;
    }
    return count;
}

std::string base64_encode(std::string_view data) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= data.size()) {
        uint32_t chunk = (static_cast<uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                         (static_cast<uint32_t>(static_cast<unsigned char>(data[i + 1]))
                          << 8) |
                         static_cast<uint32_t>(static_cast<unsigned char>(data[i + 2]));
        out.push_back(table[(chunk >> 18) & 0x3f]);
        out.push_back(table[(chunk >> 12) & 0x3f]);
        out.push_back(table[(chunk >> 6) & 0x3f]);
        out.push_back(table[chunk & 0x3f]);
        i += 3;
    }
    size_t rest = data.size() - i;
    if (rest == 1) {
        uint32_t chunk =
            static_cast<uint32_t>(static_cast<unsigned char>(data[i])) << 16;
        out.push_back(table[(chunk >> 18) & 0x3f]);
        out.push_back(table[(chunk >> 12) & 0x3f]);
        out.append("==");
    } else if (rest == 2) {
        uint32_t chunk = (static_cast<uint32_t>(static_cast<unsigned char>(data[i])) << 16) |
                         (static_cast<uint32_t>(static_cast<unsigned char>(data[i + 1]))
                          << 8);
        out.push_back(table[(chunk >> 18) & 0x3f]);
        out.push_back(table[(chunk >> 12) & 0x3f]);
        out.push_back(table[(chunk >> 6) & 0x3f]);
        out.push_back('=');
    }
    return out;
}

std::string url_quote_path(std::string_view path) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(path.size());
    for (char raw : path) {
        unsigned char c = static_cast<unsigned char>(raw);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' ||
                          c == '~' || c == '/';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0f]);
        }
    }
    return out;
}

std::string url_quote_query(std::string_view value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size());
    for (char raw : value) {
        unsigned char c = static_cast<unsigned char>(raw);
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' ||
                          c == '~';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0f]);
        }
    }
    return out;
}

std::string codepoint_prefix(std::string_view text, size_t count) {
    size_t seen = 0;
    size_t i = 0;
    while (i < text.size()) {
        if ((static_cast<unsigned char>(text[i]) >> 6) != 0x2) {
            if (seen == count) break;
            ++seen;
        }
        ++i;
    }
    // Include any trailing continuation bytes of the last kept code point.
    while (i < text.size() && (static_cast<unsigned char>(text[i]) >> 6) == 0x2) ++i;
    return std::string(text.substr(0, i));
}

}  // namespace lcpush::util
