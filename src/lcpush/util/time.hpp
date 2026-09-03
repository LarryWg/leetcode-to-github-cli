// ISO8601 UTC timestamps for the problems cache.
#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace lcpush::util {

// "2026-07-27T10:00:00Z" for an epoch second.
std::string format_iso_utc(std::time_t stamp);

// Parse the formats the cache can contain: trailing Z or +00:00, and naive
// timestamps which are treated as UTC like the Python code did.
std::optional<std::time_t> parse_iso_utc(const std::string& text);

// "HH:MM:SS TZ" in the local timezone, for the rate-limit message.
std::string format_local_hms(std::time_t stamp);

}  // namespace lcpush::util
