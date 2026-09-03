// Shared fixtures ported from the Python conftest.
#pragma once

#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "lcpush/problems.hpp"

namespace lcpush::testing {

inline constexpr const char* kPySolution =
    "class Solution:\n"
    "    def twoSum(self, nums: List[int], target: int) -> List[int]:\n"
    "        seen = {}\n"
    "        for i, n in enumerate(nums):\n"
    "            if target - n in seen:\n"
    "                return [seen[target - n], i]\n"
    "            seen[n] = i\n"
    "        return []\n";

inline constexpr const char* kCppSolution =
    "class Solution {\n"
    "public:\n"
    "    vector<int> twoSum(vector<int>& nums, int target) {\n"
    "        unordered_map<int, int> seen;\n"
    "        for (int i = 0; i < nums.size(); ++i) {\n"
    "            if (seen.count(target - nums[i])) return {seen[target - nums[i]], i};\n"
    "            seen[nums[i]] = i;\n"
    "        }\n"
    "        return {};\n"
    "    }\n"
    "};\n";

inline std::vector<Question> questions() {
    return {
        {"1", "Two Sum", "two-sum", "Easy", false},
        {"2", "Add Two Numbers", "add-two-numbers", "Medium", false},
        {"3", "Longest Substring Without Repeating Characters",
         "longest-substring-without-repeating-characters", "Medium", false},
        {"167", "Two Sum II - Input Array Is Sorted", "two-sum-ii-input-array-is-sorted",
         "Medium", false},
        {"653", "Two Sum IV - Input is a BST", "two-sum-iv-input-is-a-bst", "Easy", false},
        {"1099", "Two Sum Less Than K", "two-sum-less-than-k", "Easy", true},
    };
}

inline Question two_sum() { return questions()[0]; }

// Scoped environment variable override, restored on destruction.
class EnvVar {
  public:
    EnvVar(std::string name, const std::optional<std::string>& value)
        : name_(std::move(name)) {
        const char* current = std::getenv(name_.c_str());
        if (current != nullptr) previous_ = current;
        apply(value);
    }

    ~EnvVar() { apply(previous_); }

    EnvVar(const EnvVar&) = delete;
    EnvVar& operator=(const EnvVar&) = delete;

  private:
    void apply(const std::optional<std::string>& value) {
        if (value) {
            ::setenv(name_.c_str(), value->c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

    std::string name_;
    std::optional<std::string> previous_;
};

// Port of the autouse isolated_dirs fixture: a fresh temp directory with
// LCPUSH_CONFIG_DIR and LCPUSH_CACHE_DIR pointing into it, token env cleared.
class IsolatedDirs {
  public:
    IsolatedDirs() {
        auto base = std::filesystem::temp_directory_path() / "lcpush-tests";
        std::filesystem::create_directories(base);
        std::mt19937_64 rng{std::random_device{}()};
        root_ = base / std::to_string(rng());
        std::filesystem::create_directories(root_);
        overrides_.push_back(
            std::make_unique<EnvVar>("LCPUSH_CONFIG_DIR", (root_ / "config").string()));
        overrides_.push_back(
            std::make_unique<EnvVar>("LCPUSH_CACHE_DIR", (root_ / "cache").string()));
        overrides_.push_back(std::make_unique<EnvVar>("LCPUSH_GITHUB_TOKEN", std::nullopt));
        overrides_.push_back(std::make_unique<EnvVar>("GITHUB_TOKEN", std::nullopt));
    }

    ~IsolatedDirs() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    const std::filesystem::path& root() const { return root_; }

  private:
    std::filesystem::path root_;
    std::vector<std::unique_ptr<EnvVar>> overrides_;
};

}  // namespace lcpush::testing
