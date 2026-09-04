// Solution content: normalization, hard rejections, soft warnings.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "lcpush/domain/detect.hpp"

namespace lcpush::solution {

inline constexpr size_t kMaxBytes = 1024 * 1024;
inline constexpr size_t kLargeClipboardBytes = 200 * 1024;

// CRLF -> LF, strip trailing whitespace per line, exactly one trailing newline.
std::string normalize(const std::string& text);

int line_count(const std::string& text);
size_t byte_size(const std::string& text);

// The only two hard rejections. Both re-prompt, never exit.
std::optional<std::string> reject_reason(const std::string& text);

// Plausible entry-point function names, in source order, deduplicated.
std::vector<std::string> entry_point_names(const std::string& text);

// Rough balance check that skips string literals and comments.
bool brackets_balanced(const std::string& text);

// Does an entry-point name plausibly correspond to the question slug?
bool matches_slug(const std::string& name, const std::string& slug);

// Non-blocking warnings printed above the confirm prompt.
std::vector<std::string> soft_warnings(const std::string& text,
                                       const detect::Detection* detection = nullptr,
                                       const std::string& slug = "",
                                       const std::string& title = "");

}  // namespace lcpush::solution
