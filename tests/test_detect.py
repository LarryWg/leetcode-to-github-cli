from __future__ import annotations

import pytest

from lcpush.detect import THRESHOLD, detect, rank, resolve_language, score
from tests.conftest import CPP_SOLUTION, PY_SOLUTION

C_SOLUTION = """#include <stdlib.h>

int* twoSum(int* nums, int numsSize, int target, int* returnSize) {
    int* out = malloc(2 * sizeof(int));
    *returnSize = 2;
    return out;
}
"""

JAVA_SOLUTION = """import java.util.*;

public class Solution {
    public int[] twoSum(int[] nums, int target) {
        Map<Integer, Integer> seen = new HashMap<>();
        return new int[]{};
    }
}
"""

CSHARP_SOLUTION = """using System;
using System.Collections.Generic;

public class Solution {
    public IList<int> TwoSum(int[] nums, int target) {
        return new List<int>();
    }
}
"""

JS_SOLUTION = """/**
 * @param {number[]} nums
 * @param {number} target
 * @return {number[]}
 */
var twoSum = function(nums, target) {
    const seen = new Map();
    return [];
};
"""

TS_SOLUTION = """function twoSum(nums: number[], target: number): number[] {
    const seen = new Map<number, number>();
    return [];
}
"""

GO_SOLUTION = """package main

func twoSum(nums []int, target int) []int {
    seen := make(map[int]int)
    return []int{}
}
"""

RUST_SOLUTION = """impl Solution {
    pub fn two_sum(nums: Vec<i32>, target: i32) -> Vec<i32> {
        vec![]
    }
}
"""

RUBY_SOLUTION = """# @param {Integer[]} nums
# @param {Integer} target
# @return {Integer[]}
def two_sum(nums, target)
    seen = {}
    nums.each_with_index do |n, i|
        return [seen[target - n], i] if seen[target - n]
    end
    nil
end
"""

SWIFT_SOLUTION = """class Solution {
    func twoSum(_ nums: [Int], _ target: Int) -> [Int] {
        var seen = [Int: Int]()
        return []
    }
}
"""

KOTLIN_SOLUTION = """class Solution {
    fun twoSum(nums: IntArray, target: Int): IntArray {
        val seen = HashMap<Int, Int>()
        return intArrayOf()
    }
}
"""

SCALA_SOLUTION = """object Solution {
    def twoSum(nums: Array[Int], target: Int): Array[Int] = {
        val seen = scala.collection.mutable.Map[Int, Int]()
        Array()
    }
}
"""

PHP_SOLUTION = """<?php
class Solution {
    function twoSum($nums, $target) {
        $seen = array();
        return array();
    }
}
"""


@pytest.mark.parametrize(
    "source,expected",
    [
        (PY_SOLUTION, "python3"),
        (CPP_SOLUTION, "cpp"),
        (C_SOLUTION, "c"),
        (JAVA_SOLUTION, "java"),
        (CSHARP_SOLUTION, "csharp"),
        (JS_SOLUTION, "javascript"),
        (TS_SOLUTION, "typescript"),
        (GO_SOLUTION, "golang"),
        (RUST_SOLUTION, "rust"),
        (RUBY_SOLUTION, "ruby"),
        (SWIFT_SOLUTION, "swift"),
        (KOTLIN_SOLUTION, "kotlin"),
        (SCALA_SOLUTION, "scala"),
        (PHP_SOLUTION, "php"),
    ],
)
def test_detects_each_language(source, expected):
    detection = detect(source)
    assert detection.confident
    assert detection.language.key == expected


def test_c_is_ruled_out_when_cpp_markers_present():
    assert score(CPP_SOLUTION)["c"] == 0


def test_using_system_forces_csharp_over_java():
    scores = score(CSHARP_SOLUTION)
    assert scores["csharp"] > scores["java"]


def test_type_annotations_force_typescript_over_javascript():
    scores = score(TS_SOLUTION)
    assert scores["typescript"] > scores["javascript"]


def test_plain_js_stays_javascript():
    scores = score(JS_SOLUTION)
    assert scores["javascript"] > scores["typescript"]


def test_prose_detects_nothing():
    detection = detect("hey are we still on for lunch tomorrow at noon")
    assert not detection.confident
    assert detection.label == "unknown"
    assert detection.score < THRESHOLD


def test_empty_text_detects_nothing():
    assert detect("   ").language is None


def test_rank_returns_every_language_sorted():
    ranked = rank(PY_SOLUTION)
    assert len(ranked) == 14
    assert ranked[0][0].key == "python3"
    assert [pair[1] for pair in ranked] == sorted(
        (pair[1] for pair in ranked), reverse=True
    )


@pytest.mark.parametrize(
    "alias,key",
    [("py", "python3"), ("python", "python3"), ("c++", "cpp"), ("go", "golang"),
     ("C#", "csharp"), ("TS", "typescript")],
)
def test_language_aliases(alias, key):
    assert resolve_language(alias).key == key


def test_unknown_language_alias():
    assert resolve_language("brainfuck") is None


def test_extensions_match_spec():
    assert resolve_language("python3").ext == ".py"
    assert resolve_language("cpp").ext == ".cpp"
    assert resolve_language("golang").ext == ".go"
