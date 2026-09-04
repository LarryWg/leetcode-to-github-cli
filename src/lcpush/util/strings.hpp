// Small string helpers shared across modules. UTF-8 aware where noted.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lcpush::util {

std::string trim(std::string_view text);
std::string rstrip(std::string_view text);
std::string rstrip(std::string_view text, char what);
std::string strip_chars(std::string_view text, std::string_view chars);
std::string to_lower(std::string_view text);
bool starts_with(std::string_view text, std::string_view prefix);
bool ends_with(std::string_view text, std::string_view suffix);
std::string replace_all(std::string text, std::string_view from, std::string_view to);

// Split on a single character, keeping empty parts (Python str.split with sep).
std::vector<std::string> split(std::string_view text, char sep);

// Join parts with a separator.
std::string join(const std::vector<std::string>& parts, std::string_view sep);

// Zero-pad like Python str.zfill: never truncates.
std::string zfill(std::string_view text, size_t width);

bool is_all_digits(std::string_view text);

// Decode UTF-8 to code points. Invalid bytes decode as one replacement each,
// which keeps counting monotonic for the display-width math.
std::u32string to_u32(std::string_view text);

// Number of code points, matching Python len() on str.
size_t codepoint_count(std::string_view text);

// Standard base64 of raw bytes.
std::string base64_encode(std::string_view data);

// Percent-encode a URL path like Python quote(path, safe="/").
std::string url_quote_path(std::string_view path);

// Percent-encode a URL query value with no reserved characters left unescaped.
std::string url_quote_query(std::string_view value);

// First `count` code points as a UTF-8 string, matching Python slicing.
std::string codepoint_prefix(std::string_view text, size_t count);

}  // namespace lcpush::util
