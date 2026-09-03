#include <catch2/catch_test_macros.hpp>

#include "helpers/fixtures.hpp"
#include "lcpush/ui.hpp"

using namespace lcpush;
using lcpush::testing::kPySolution;

namespace {

std::string long_text() {
    std::string out;
    for (int i = 1; i <= 24; ++i) out += "line " + std::to_string(i) + "\n";
    return out;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("preview shows head and tail with hidden count") {
    auto lines = ui::preview_lines(long_text());
    REQUIRE(lines.size() == 9);
    CHECK(lines[0] == "line 1");
    CHECK(lines[4] == "line 5");
    CHECK(lines[5] == " … 16 lines hidden …");
    CHECK(lines[6] == "line 22");
    CHECK(lines[8] == "line 24");
}

TEST_CASE("short content is shown whole") {
    CHECK(ui::preview_lines("a\nb\nc\n") == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("preview header carries counts and language") {
    auto box = ui::render_preview("Clipboard", kPySolution, "Python3");
    CHECK(contains(box, "┌ Clipboard — 8 lines,"));
    CHECK(contains(box, "bytes, detected Python3"));
    CHECK(contains(box, "class Solution:"));
}

TEST_CASE("preview header says unknown when undetected") {
    CHECK(contains(ui::render_preview("Clipboard", "hello there\n", "unknown"),
                   "detected unknown"));
}

TEST_CASE("push panel add") {
    auto panel = ui::render_push_panel({.filename = "0001-two-sum.py",
                                        .lines = 24,
                                        .repo = "user/leetcode-solutions",
                                        .branch = "main",
                                        .message = "Add 1. Two Sum (Python3)",
                                        .updating = false});
    CHECK(contains(panel, "┌ Ready to push"));
    CHECK_FALSE(contains(panel, "overwrites"));
    CHECK(contains(panel, "File     0001-two-sum.py  (24 lines)"));
    CHECK(contains(panel, "Repo     user/leetcode-solutions  (main)"));
    CHECK(contains(panel, "Message  Add 1. Two Sum (Python3)"));
    CHECK(contains(panel, "[Enter] push"));
    CHECK(contains(panel, "[m] edit message"));
    CHECK(contains(panel, "[n] cancel"));
}

TEST_CASE("push panel overwrite header") {
    auto panel = ui::render_push_panel({.filename = "0001-two-sum.py",
                                        .lines = 24,
                                        .repo = "user/solutions",
                                        .branch = "main",
                                        .message = "Update 1. Two Sum (Python3)",
                                        .updating = true});
    CHECK(contains(panel, "┌ Ready to push (overwrites existing file)"));
}

TEST_CASE("push panel hides edit keys when prompt never") {
    auto panel = ui::render_push_panel({.filename = "f.py",
                                        .lines = 1,
                                        .repo = "user/solutions",
                                        .branch = "main",
                                        .message = "m",
                                        .updating = false,
                                        .prompt_mode = "never"});
    CHECK_FALSE(contains(panel, "[m] edit message"));
    CHECK(contains(panel, "[Enter] push"));
}

TEST_CASE("push panel renders multiline body") {
    auto panel = ui::render_push_panel({.filename = "f.py",
                                        .lines = 1,
                                        .repo = "user/solutions",
                                        .branch = "main",
                                        .message = "subject\n\nbody detail",
                                        .updating = false});
    CHECK(contains(panel, "Message  subject"));
    CHECK(contains(panel, "body detail"));
}
