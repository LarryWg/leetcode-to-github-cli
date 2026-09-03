#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "helpers/fixtures.hpp"
#include "lcpush/search.hpp"

using namespace lcpush;

namespace {

std::vector<std::string> ids(const std::vector<Question>& results) {
    std::vector<std::string> out;
    for (const Question& q : results) out.push_back(q.id);
    return out;
}

std::vector<std::string> slugs(const std::vector<Question>& results) {
    std::vector<std::string> out;
    for (const Question& q : results) out.push_back(q.slug);
    return out;
}

}  // namespace

TEST_CASE("empty query lists by id") {
    auto index = search::build_index(testing::questions());
    auto results = search::search(index, "", 3);
    CHECK(ids(results) == std::vector<std::string>{"1", "2", "3"});
}

TEST_CASE("prefix typing filters offline") {
    auto index = search::build_index(testing::questions());
    auto found = slugs(search::search(index, "two su"));
    CHECK(std::find(found.begin(), found.end(), "two-sum") != found.end());
    CHECK(std::find(found.begin(), found.end(), "two-sum-ii-input-array-is-sorted") !=
          found.end());
    REQUIRE(found.size() >= 2);
    CHECK(found[0] != "add-two-numbers");
    CHECK(found[1] != "add-two-numbers");
}

TEST_CASE("digit query prefix matches id first") {
    auto index = search::build_index(testing::questions());
    auto results = search::search(index, "1");
    REQUIRE(results.size() >= 3);
    CHECK(ids({results[0], results[1], results[2]}) ==
          std::vector<std::string>{"1", "167", "1099"});
}

TEST_CASE("digit query for specific id") {
    auto index = search::build_index(testing::questions());
    auto results = search::search(index, "167");
    REQUIRE_FALSE(results.empty());
    CHECK(results[0].id == "167");
}

TEST_CASE("slug query matches") {
    auto index = search::build_index(testing::questions());
    auto results = search::search(index, "longest-substring");
    REQUIRE_FALSE(results.empty());
    CHECK(results[0].id == "3");
}

TEST_CASE("limit is respected") {
    auto index = search::build_index(testing::questions());
    CHECK(search::search(index, "two", 2).size() == 2);
}

TEST_CASE("no matches returns empty") {
    auto index = search::build_index(testing::questions());
    CHECK(search::search(index, "zzzzqqqqxxxx").empty());
}
