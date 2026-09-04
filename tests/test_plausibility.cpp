#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "helpers/fixtures.hpp"
#include "lcpush/domain/plausibility.hpp"

using namespace lcpush;
using lcpush::testing::kCppSolution;
using lcpush::testing::kPySolution;

namespace {

bool has_reason(const plausibility::Plausibility& result, const std::string& needle) {
    return std::any_of(result.reasons.begin(), result.reasons.end(),
                       [&](const std::string& reason) {
                           return reason.find(needle) != std::string::npos;
                       });
}

}  // namespace

TEST_CASE("solution is plausible") {
    CHECK(plausibility::assess(std::string(kPySolution)).plausible);
    CHECK(plausibility::assess(std::string(kCppSolution)).plausible);
}

TEST_CASE("empty clipboard is not plausible") {
    auto result = plausibility::assess(std::string(""));
    CHECK_FALSE(result.plausible);
    CHECK(result.reasons == std::vector<std::string>{"clipboard is empty"});
    CHECK_FALSE(plausibility::assess(std::nullopt).plausible);
}

TEST_CASE("bare url is not plausible") {
    auto result = plausibility::assess(std::string("https://leetcode.com/problems/two-sum/"));
    CHECK_FALSE(result.plausible);
    CHECK(has_reason(result, "bare URL"));
}

TEST_CASE("email and path are not plausible") {
    CHECK_FALSE(plausibility::assess(std::string("someone@example.com")).plausible);
    CHECK_FALSE(plausibility::assess(std::string("/Users/larry/Downloads/notes.txt")).plausible);
}

TEST_CASE("short single line is not plausible") {
    CHECK_FALSE(plausibility::assess(std::string("two sum")).plausible);
}

TEST_CASE("prose is not plausible") {
    std::string prose =
        "Hey, I was reading about the two sum problem yesterday and it turns out "
        "the hash map approach is by far the most common way people solve it "
        "during interviews these days";
    CHECK_FALSE(plausibility::assess(prose).plausible);
}

TEST_CASE("huge content is penalized") {
    std::string big = std::string(kPySolution) + "\n";
    for (int i = 0; i < 40000; ++i) big += "# padding\n";
    auto result = plausibility::assess(big);
    CHECK(has_reason(result, "200KB"));
}
