#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "helpers/fixtures.hpp"
#include "lcpush/detect.hpp"

using namespace lcpush;
using lcpush::testing::kCppSolution;
using lcpush::testing::kPySolution;

namespace {

const char* kCSolution =
    "#include <stdlib.h>\n\n"
    "int* twoSum(int* nums, int numsSize, int target, int* returnSize) {\n"
    "    int* out = malloc(2 * sizeof(int));\n"
    "    *returnSize = 2;\n"
    "    return out;\n"
    "}\n";

const char* kJavaSolution =
    "import java.util.*;\n\n"
    "public class Solution {\n"
    "    public int[] twoSum(int[] nums, int target) {\n"
    "        Map<Integer, Integer> seen = new HashMap<>();\n"
    "        return new int[]{};\n"
    "    }\n"
    "}\n";

const char* kCSharpSolution =
    "using System;\n"
    "using System.Collections.Generic;\n\n"
    "public class Solution {\n"
    "    public IList<int> TwoSum(int[] nums, int target) {\n"
    "        return new List<int>();\n"
    "    }\n"
    "}\n";

const char* kJsSolution =
    "/**\n"
    " * @param {number[]} nums\n"
    " * @param {number} target\n"
    " * @return {number[]}\n"
    " */\n"
    "var twoSum = function(nums, target) {\n"
    "    const seen = new Map();\n"
    "    return [];\n"
    "};\n";

const char* kTsSolution =
    "function twoSum(nums: number[], target: number): number[] {\n"
    "    const seen = new Map<number, number>();\n"
    "    return [];\n"
    "}\n";

const char* kGoSolution =
    "package main\n\n"
    "func twoSum(nums []int, target int) []int {\n"
    "    seen := make(map[int]int)\n"
    "    return []int{}\n"
    "}\n";

const char* kRustSolution =
    "impl Solution {\n"
    "    pub fn two_sum(nums: Vec<i32>, target: i32) -> Vec<i32> {\n"
    "        vec![]\n"
    "    }\n"
    "}\n";

const char* kRubySolution =
    "# @param {Integer[]} nums\n"
    "# @param {Integer} target\n"
    "# @return {Integer[]}\n"
    "def two_sum(nums, target)\n"
    "    seen = {}\n"
    "    nums.each_with_index do |n, i|\n"
    "        return [seen[target - n], i] if seen[target - n]\n"
    "    end\n"
    "    nil\n"
    "end\n";

const char* kSwiftSolution =
    "class Solution {\n"
    "    func twoSum(_ nums: [Int], _ target: Int) -> [Int] {\n"
    "        var seen = [Int: Int]()\n"
    "        return []\n"
    "    }\n"
    "}\n";

const char* kKotlinSolution =
    "class Solution {\n"
    "    fun twoSum(nums: IntArray, target: Int): IntArray {\n"
    "        val seen = HashMap<Int, Int>()\n"
    "        return intArrayOf()\n"
    "    }\n"
    "}\n";

const char* kScalaSolution =
    "object Solution {\n"
    "    def twoSum(nums: Array[Int], target: Int): Array[Int] = {\n"
    "        val seen = scala.collection.mutable.Map[Int, Int]()\n"
    "        Array()\n"
    "    }\n"
    "}\n";

const char* kPhpSolution =
    "<?php\n"
    "class Solution {\n"
    "    function twoSum($nums, $target) {\n"
    "        $seen = array();\n"
    "        return array();\n"
    "    }\n"
    "}\n";

}  // namespace

TEST_CASE("detects each language") {
    auto [source, expected] = GENERATE_COPY(table<const char*, const char*>({
        {kPySolution, "python3"},
        {kCppSolution, "cpp"},
        {kCSolution, "c"},
        {kJavaSolution, "java"},
        {kCSharpSolution, "csharp"},
        {kJsSolution, "javascript"},
        {kTsSolution, "typescript"},
        {kGoSolution, "golang"},
        {kRustSolution, "rust"},
        {kRubySolution, "ruby"},
        {kSwiftSolution, "swift"},
        {kKotlinSolution, "kotlin"},
        {kScalaSolution, "scala"},
        {kPhpSolution, "php"},
    }));
    auto detection = detect::detect(source);
    INFO("expected " << expected);
    REQUIRE(detection.confident());
    CHECK(detection.language->key == expected);
}

TEST_CASE("c is ruled out when cpp markers present") {
    CHECK(detect::score(kCppSolution)["c"] == 0);
}

TEST_CASE("using System forces csharp over java") {
    auto scores = detect::score(kCSharpSolution);
    CHECK(scores["csharp"] > scores["java"]);
}

TEST_CASE("type annotations force typescript over javascript") {
    auto scores = detect::score(kTsSolution);
    CHECK(scores["typescript"] > scores["javascript"]);
}

TEST_CASE("plain js stays javascript") {
    auto scores = detect::score(kJsSolution);
    CHECK(scores["javascript"] > scores["typescript"]);
}

TEST_CASE("prose detects nothing") {
    auto detection = detect::detect("hey are we still on for lunch tomorrow at noon");
    CHECK_FALSE(detection.confident());
    CHECK(detection.label() == "unknown");
    CHECK(detection.score < detect::kThreshold);
}

TEST_CASE("empty text detects nothing") {
    CHECK_FALSE(detect::detect("   ").language.has_value());
}

TEST_CASE("rank returns every language sorted") {
    auto ranked = detect::rank(kPySolution);
    REQUIRE(ranked.size() == 14);
    CHECK(ranked[0].first.key == "python3");
    for (size_t i = 1; i < ranked.size(); ++i) {
        CHECK(ranked[i - 1].second >= ranked[i].second);
    }
}

TEST_CASE("language aliases") {
    auto [alias, key] = GENERATE(table<const char*, const char*>({
        {"py", "python3"},
        {"python", "python3"},
        {"c++", "cpp"},
        {"go", "golang"},
        {"C#", "csharp"},
        {"TS", "typescript"},
    }));
    auto* language = detect::resolve_language(alias);
    REQUIRE(language != nullptr);
    CHECK(language->key == key);
}

TEST_CASE("unknown language alias") {
    CHECK(detect::resolve_language("brainfuck") == nullptr);
}

TEST_CASE("extensions match spec") {
    CHECK(detect::resolve_language("python3")->ext == ".py");
    CHECK(detect::resolve_language("cpp")->ext == ".cpp");
    CHECK(detect::resolve_language("golang")->ext == ".go");
}
