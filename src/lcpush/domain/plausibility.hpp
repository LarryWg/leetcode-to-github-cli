// Clipboard plausibility scoring. Decides menu ordering only, never blocks.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lcpush::plausibility {

inline constexpr int kPlausibleThreshold = 3;

struct Plausibility {
    bool plausible = false;
    int score = 0;
    std::vector<std::string> reasons;
};

// Score clipboard content as "looks like a LeetCode solution" or not.
Plausibility assess(const std::optional<std::string>& text);

}  // namespace lcpush::plausibility
