// Fuzzy problem search. Pure, offline, runs on every keystroke.
#pragma once

#include <string>
#include <vector>

#include "lcpush/leetcode/problems.hpp"

namespace lcpush::search {

inline constexpr double kScoreCutoff = 55.0;

struct ProblemIndex {
    std::vector<Question> questions;
    std::vector<std::string> displays;
    std::vector<std::string> slugs;
};

ProblemIndex build_index(const std::vector<Question>& questions);

// Top limit matches for query, best first. An all-digit query exact-prefix
// matches on the problem id first, anything else is fuzzy-matched against
// both "{id}. {title}" and the slug, taking the better of the two scores.
std::vector<Question> search(const ProblemIndex& index, const std::string& query,
                             int limit = 10);

}  // namespace lcpush::search
