#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "helpers/fixtures.hpp"
#include "lcpush/domain/detect.hpp"
#include "lcpush/domain/solution.hpp"

using namespace lcpush;
using lcpush::testing::kCppSolution;
using lcpush::testing::kPySolution;

namespace {

bool any_contains(const std::vector<std::string>& warnings, const std::string& needle) {
    return std::any_of(warnings.begin(), warnings.end(), [&](const std::string& w) {
        return w.find(needle) != std::string::npos;
    });
}

}  // namespace

TEST_CASE("normalize converts crlf and trims trailing space") {
    CHECK(solution::normalize("a  \r\nb\t\r\n") == "a\nb\n");
}

TEST_CASE("normalize ensures exactly one trailing newline") {
    CHECK(solution::normalize("x\n\n\n\n") == "x\n");
    CHECK(solution::normalize("x") == "x\n");
}

TEST_CASE("normalize preserves content byte for byte") {
    CHECK(solution::normalize(kCppSolution) == kCppSolution);
}

TEST_CASE("normalize empty") {
    CHECK(solution::normalize("   \n\n") == "");
}

TEST_CASE("line and byte counts") {
    CHECK(solution::line_count(kPySolution) == 8);
    CHECK(solution::byte_size("héllo") == 6);
}

TEST_CASE("reject empty") {
    auto reason = solution::reject_reason("   \n ");
    REQUIRE(reason.has_value());
    CHECK(*reason == "Content is empty.");
}

TEST_CASE("reject oversized") {
    std::string big(solution::kMaxBytes + 1, 'x');
    auto reason = solution::reject_reason(big);
    REQUIRE(reason.has_value());
    CHECK(reason->find("1MB") != std::string::npos);
}

TEST_CASE("accepts normal solution") {
    CHECK_FALSE(solution::reject_reason(kPySolution).has_value());
}

TEST_CASE("entry point names python") {
    auto names = solution::entry_point_names(kPySolution);
    CHECK(std::find(names.begin(), names.end(), "twoSum") != names.end());
}

TEST_CASE("entry point names cpp") {
    auto names = solution::entry_point_names(kCppSolution);
    CHECK(std::find(names.begin(), names.end(), "twoSum") != names.end());
}

TEST_CASE("entry point names javascript") {
    auto names = solution::entry_point_names(
        "var lengthOfLongestSubstring = function(s) { return 0; };");
    REQUIRE(names.size() == 1);
    CHECK(names[0] == "lengthOfLongestSubstring");
}

TEST_CASE("brackets balanced") {
    CHECK(solution::brackets_balanced(kPySolution));
    CHECK(solution::brackets_balanced(kCppSolution));
}

TEST_CASE("brackets unbalanced on truncation") {
    std::string full = kCppSolution;
    std::string half = full.substr(0, full.size() / 2);
    CHECK_FALSE(solution::brackets_balanced(half));
}

TEST_CASE("brackets ignore literals and comments") {
    CHECK(solution::brackets_balanced("x = \"{{{\"  # )))\n"));
    CHECK(solution::brackets_balanced("/* ( */ int a = 1;\n"));
}

TEST_CASE("matches slug camel and snake") {
    CHECK(solution::matches_slug("twoSum", "two-sum"));
    CHECK(solution::matches_slug("two_sum", "two-sum"));
    CHECK(solution::matches_slug("TwoSum", "two-sum"));
    CHECK_FALSE(solution::matches_slug("lengthOfLongestSubstring", "two-sum"));
}

TEST_CASE("matches slug is permissive without a slug") {
    CHECK(solution::matches_slug("whatever", ""));
}

TEST_CASE("warning on unknown language") {
    auto detection = detect::detect("just some words");
    auto warnings = solution::soft_warnings("just some words here", &detection);
    CHECK(any_contains(warnings, "Could not identify"));
}

TEST_CASE("warning on wrong question") {
    std::string code = "var lengthOfLongestSubstring = function(s) { return 0; };\n";
    auto warnings = solution::soft_warnings(code, nullptr, "two-sum", "Two Sum");
    bool hit = std::any_of(warnings.begin(), warnings.end(), [](const std::string& w) {
        return w.find("lengthOfLongestSubstring") != std::string::npos &&
               w.find("Two Sum") != std::string::npos;
    });
    CHECK(hit);
}

TEST_CASE("no wrong question warning when names line up") {
    auto warnings = solution::soft_warnings(kPySolution, nullptr, "two-sum", "Two Sum");
    CHECK_FALSE(any_contains(warnings, "Wrong question"));
}

TEST_CASE("warning on conflict markers and todo") {
    auto warnings = solution::soft_warnings("<<<<<<< HEAD\nTODO: finish\n");
    CHECK(any_contains(warnings, "conflict markers or TODOs"));
}

TEST_CASE("warning on truncated paste") {
    std::string full = kCppSolution;
    std::string half = full.substr(0, full.size() / 2);
    auto detection = detect::detect(half);
    auto warnings = solution::soft_warnings(half, &detection, "two-sum");
    CHECK(any_contains(warnings, "Brackets are unbalanced"));
}

TEST_CASE("clean solution has no warnings") {
    auto detection = detect::detect(kPySolution);
    CHECK(solution::soft_warnings(kPySolution, &detection, "two-sum").empty());
}
