#include "lcpush/util/time.hpp"

#include <cstdio>
#include <cstring>

namespace lcpush::util {

std::string format_iso_utc(std::time_t stamp) {
    std::tm parts{};
    ::gmtime_r(&stamp, &parts);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour,
                  parts.tm_min, parts.tm_sec);
    return buffer;
}

std::optional<std::time_t> parse_iso_utc(const std::string& text) {
    std::tm parts{};
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    int matched = std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day,
                              &hour, &minute, &second);
    if (matched < 6) return std::nullopt;
    parts.tm_year = year - 1900;
    parts.tm_mon = month - 1;
    parts.tm_mday = day;
    parts.tm_hour = hour;
    parts.tm_min = minute;
    parts.tm_sec = second;
    return ::timegm(&parts);
}

std::string format_local_hms(std::time_t stamp) {
    std::tm parts{};
    ::localtime_r(&stamp, &parts);
    char buffer[64];
    if (std::strftime(buffer, sizeof(buffer), "%H:%M:%S %Z", &parts) == 0) return "";
    return buffer;
}

}  // namespace lcpush::util
