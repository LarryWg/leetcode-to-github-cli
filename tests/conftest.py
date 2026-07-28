from __future__ import annotations

import pytest

from lcpush.detect import BY_KEY
from lcpush.problems import Question

PY_SOLUTION = """class Solution:
    def twoSum(self, nums: List[int], target: int) -> List[int]:
        seen = {}
        for i, n in enumerate(nums):
            if target - n in seen:
                return [seen[target - n], i]
            seen[n] = i
        return []
"""

CPP_SOLUTION = """class Solution {
public:
    vector<int> twoSum(vector<int>& nums, int target) {
        unordered_map<int, int> seen;
        for (int i = 0; i < nums.size(); ++i) {
            if (seen.count(target - nums[i])) return {seen[target - nums[i]], i};
            seen[nums[i]] = i;
        }
        return {};
    }
};
"""


@pytest.fixture(autouse=True)
def isolated_dirs(tmp_path, monkeypatch):
    """Never touch the real ~/.config or ~/.cache during tests."""
    monkeypatch.setenv("LCPUSH_CONFIG_DIR", str(tmp_path / "config"))
    monkeypatch.setenv("LCPUSH_CACHE_DIR", str(tmp_path / "cache"))
    monkeypatch.delenv("LCPUSH_GITHUB_TOKEN", raising=False)
    monkeypatch.delenv("GITHUB_TOKEN", raising=False)
    return tmp_path


@pytest.fixture
def questions():
    return (
        Question("1", "Two Sum", "two-sum", "Easy"),
        Question("2", "Add Two Numbers", "add-two-numbers", "Medium"),
        Question("3", "Longest Substring Without Repeating Characters",
                 "longest-substring-without-repeating-characters", "Medium"),
        Question("167", "Two Sum II - Input Array Is Sorted",
                 "two-sum-ii-input-array-is-sorted", "Medium"),
        Question("653", "Two Sum IV - Input is a BST", "two-sum-iv-input-is-a-bst", "Easy"),
        Question("1099", "Two Sum Less Than K", "two-sum-less-than-k", "Easy", paid=True),
    )


@pytest.fixture
def two_sum(questions):
    return questions[0]


@pytest.fixture
def python3():
    return BY_KEY["python3"]
