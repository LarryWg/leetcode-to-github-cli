// Replays vectors generated from the Python implementation so ranking and
// scoring behavior is provably identical, not just plausible.
#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <fstream>

#include "lcpush/detect.hpp"
#include "lcpush/plausibility.hpp"
#include "lcpush/search.hpp"
#include "lcpush/solution.hpp"

using namespace lcpush;
using nlohmann::json;

namespace {

const json& parity() {
    static const json data = [] {
        std::ifstream in(std::string(LCPUSH_TEST_DATA_DIR) + "/parity.json");
        REQUIRE(in.good());
        return json::parse(in);
    }();
    return data;
}

std::vector<Question> load_questions(const json& rows) {
    std::vector<Question> questions;
    for (const auto& row : rows) {
        questions.push_back({row["id"], row["title"], row["slug"], row["difficulty"],
                             row["paid"]});
    }
    return questions;
}

}  // namespace

TEST_CASE("detect scores match python") {
    for (const auto& item : parity()["detect"]) {
        INFO("sample: " << item["name"]);
        std::string text = item["text"];
        auto scores = detect::score(text);
        for (const auto& [key, expected] : item["scores"].items()) {
            INFO("language: " << key);
            CHECK(scores[key] == expected.get<int>());
        }
        auto detection = detect::detect(text);
        if (item["detected"].is_null()) {
            CHECK_FALSE(detection.confident());
        } else {
            REQUIRE(detection.confident());
            CHECK(detection.language->key == item["detected"].get<std::string>());
        }
        CHECK(detection.score == item["score"].get<int>());
        std::vector<std::string> ranked_keys;
        for (const auto& [lang, _] : detection.ranked) ranked_keys.push_back(lang.key);
        CHECK(ranked_keys == item["ranked_keys"].get<std::vector<std::string>>());
    }
}

TEST_CASE("plausibility scores match python") {
    for (size_t i = 0; i < parity()["plausibility"].size(); ++i) {
        const auto& item = parity()["plausibility"][i];
        INFO("sample: " << item["name"]);
        std::string text = parity()["detect"][i]["text"];
        auto result = plausibility::assess(text);
        CHECK(result.score == item["score"].get<int>());
        CHECK(result.plausible == item["plausible"].get<bool>());
        CHECK(result.reasons == item["reasons"].get<std::vector<std::string>>());
    }
}

TEST_CASE("matches_slug verdicts match python") {
    for (const auto& item : parity()["matches_slug"]) {
        INFO(item["name"] << " vs " << item["slug"]);
        CHECK(solution::matches_slug(item["name"], item["slug"]) ==
              item["matches"].get<bool>());
    }
}

TEST_CASE("search over fixture set matches python") {
    auto index = search::build_index(load_questions(parity()["subset_questions"]));
    // The fixture-set vectors use the conftest questions baked into the file.
    std::vector<Question> fixtures = {
        {"1", "Two Sum", "two-sum", "Easy", false},
        {"2", "Add Two Numbers", "add-two-numbers", "Medium", false},
        {"3", "Longest Substring Without Repeating Characters",
         "longest-substring-without-repeating-characters", "Medium", false},
        {"167", "Two Sum II - Input Array Is Sorted", "two-sum-ii-input-array-is-sorted",
         "Medium", false},
        {"653", "Two Sum IV - Input is a BST", "two-sum-iv-input-is-a-bst", "Easy", false},
        {"1099", "Two Sum Less Than K", "two-sum-less-than-k", "Easy", true},
    };
    auto fixture_index = search::build_index(fixtures);
    for (const auto& item : parity()["search_fixtures"]) {
        INFO("query: \"" << item["query"].get<std::string>() << "\"");
        std::vector<std::string> ids;
        for (const auto& q : search::search(fixture_index, item["query"])) {
            ids.push_back(q.id);
        }
        CHECK(ids == item["ids"].get<std::vector<std::string>>());
    }
}

TEST_CASE("search over real problem subset matches python") {
    auto index = search::build_index(load_questions(parity()["subset_questions"]));
    for (const auto& item : parity()["search_subset"]) {
        INFO("query: \"" << item["query"].get<std::string>() << "\"");
        std::vector<std::string> ids;
        for (const auto& q : search::search(index, item["query"])) {
            ids.push_back(q.id);
        }
        CHECK(ids == item["ids"].get<std::vector<std::string>>());
    }
}
