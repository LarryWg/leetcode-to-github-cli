// The terminal UIs, driven through scripted key bytes (no real terminal).
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "helpers/fixtures.hpp"
#include "helpers/scripted_terminal.hpp"
#include "lcpush/errors.hpp"
#include "lcpush/picker.hpp"
#include "lcpush/prompts.hpp"
#include "lcpush/search.hpp"
#include "lcpush/util/strings.hpp"

using namespace lcpush;
using lcpush::testing::ScriptedTerminal;

namespace {

std::vector<prompts::Choice> source_choices() {
    return {
        {"Clipboard (doesn't look like code)", "clipboard"},
        {"Editor", "editor"},
        {"Stdin", "stdin"},
    };
}

}  // namespace

TEST_CASE("typing filters and enter selects") {
    auto index = search::build_index(testing::questions());
    ScriptedTerminal terminal("two su\r");
    auto chosen = picker::pick(terminal.terminal(), index);
    CHECK(chosen.slug == "two-sum");
}

TEST_CASE("arrow keys move the cursor") {
    auto index = search::build_index(testing::questions());
    ScriptedTerminal terminal("two su\x1b[B\r");
    auto chosen = picker::pick(terminal.terminal(), index);
    CHECK(chosen.slug != "two-sum");
    CHECK(chosen.slug.find("two-sum") != std::string::npos);
}

TEST_CASE("digits jump to an id") {
    auto index = search::build_index(testing::questions());
    ScriptedTerminal terminal("167\r");
    CHECK(picker::pick(terminal.terminal(), index).id == "167");
}

TEST_CASE("escape cancels the picker") {
    auto index = search::build_index(testing::questions());
    ScriptedTerminal terminal("\x1b");
    CHECK_THROWS_AS(picker::pick(terminal.terminal(), index), Cancelled);
}

TEST_CASE("ctrl c cancels the picker") {
    auto index = search::build_index(testing::questions());
    ScriptedTerminal terminal("\x03");
    CHECK_THROWS_AS(picker::pick(terminal.terminal(), index), Cancelled);
}

TEST_CASE("empty index is cancelled") {
    auto index = search::build_index({});
    ScriptedTerminal terminal("");
    CHECK_THROWS_AS(picker::pick(terminal.terminal(), index), Cancelled);
}

TEST_CASE("format row right aligns difficulty and marks paid") {
    auto questions = testing::questions();
    auto row = picker::format_row(questions[0], 60);
    CHECK(row.rfind("1. Two Sum", 0) == 0);
    CHECK(util::rstrip(row).ends_with("[Easy]"));
    CHECK(util::rstrip(picker::format_row(questions.back(), 60)).ends_with("[Easy] 🔒"));
}

TEST_CASE("format row truncates long titles") {
    auto questions = testing::questions();
    auto row = picker::format_row(questions[2], 48);
    CHECK(row.find("…") != std::string::npos);
    CHECK(util::rstrip(row).ends_with("[Medium]"));
}

TEST_CASE("read key maps each panel key") {
    auto [keys, expected] = GENERATE(table<const char*, const char*>({
        {"\r", "push"},
        {"m", "edit"},
        {"M", "editor"},
        {"n", "cancel"},
    }));
    ScriptedTerminal terminal(keys);
    std::map<std::string, std::string> allowed = {
        {"enter", "push"}, {"m", "edit"}, {"M", "editor"}, {"n", "cancel"}};
    CHECK(prompts::read_key(terminal.terminal(), allowed) == expected);
}

TEST_CASE("read key ctrl c cancels") {
    ScriptedTerminal terminal("\x03");
    CHECK_THROWS_AS(prompts::read_key(terminal.terminal(), {{"enter", "push"}}), Cancelled);
}

TEST_CASE("select honours a value default") {
    ScriptedTerminal terminal("\r");
    auto chosen =
        prompts::select(terminal.terminal(), "? Solution source:", source_choices(), "editor");
    CHECK(chosen == "editor");
}

TEST_CASE("select arrow then enter") {
    ScriptedTerminal terminal("\x1b[B\r");
    auto chosen =
        prompts::select(terminal.terminal(), "? Solution source:", source_choices(), "editor");
    CHECK(chosen == "stdin");
}

TEST_CASE("select ctrl c becomes cancelled") {
    ScriptedTerminal terminal("\x03");
    CHECK_THROWS_AS(
        prompts::select(terminal.terminal(), "? Solution source:", source_choices()),
        Cancelled);
}

TEST_CASE("confirm defaults to yes on enter") {
    {
        ScriptedTerminal terminal("\r");
        CHECK(prompts::confirm(terminal.terminal(), "  └ Use this?", true) == true);
    }
    {
        ScriptedTerminal terminal("n");
        CHECK(prompts::confirm(terminal.terminal(), "  └ Use this?", true) == false);
    }
}

TEST_CASE("edit line starts pre filled") {
    ScriptedTerminal terminal("\r");
    CHECK(prompts::edit_line(terminal.terminal(), "? Commit message:  ", "Add 1. Two Sum") ==
          "Add 1. Two Sum");
}

TEST_CASE("edit line appends to the prefill") {
    ScriptedTerminal terminal(" via hash map\r");
    CHECK(prompts::edit_line(terminal.terminal(), "? Commit message:  ", "Add 1. Two Sum") ==
          "Add 1. Two Sum via hash map");
}

TEST_CASE("edit line ctrl u clears") {
    ScriptedTerminal terminal("\x15Rewritten\r");
    CHECK(prompts::edit_line(terminal.terminal(), "? Commit message:  ", "Add 1. Two Sum") ==
          "Rewritten");
}

TEST_CASE("key decoder handles sequences and utf8") {
    using term::Key;
    ScriptedTerminal terminal("a\x1b[A\x1b[B\x1b[C\x1b[D\x1b[H\x1b[F\x1b[1~\x1b[4~é🔒\x7f\x04");
    auto& keys = terminal.terminal().keys;
    CHECK(keys.next() == Key::ch("a"));
    CHECK(keys.next() == Key::of(Key::Type::Up));
    CHECK(keys.next() == Key::of(Key::Type::Down));
    CHECK(keys.next() == Key::of(Key::Type::Right));
    CHECK(keys.next() == Key::of(Key::Type::Left));
    CHECK(keys.next() == Key::of(Key::Type::Home));
    CHECK(keys.next() == Key::of(Key::Type::End));
    CHECK(keys.next() == Key::of(Key::Type::Home));
    CHECK(keys.next() == Key::of(Key::Type::End));
    CHECK(keys.next() == Key::ch("é"));
    CHECK(keys.next() == Key::ch("🔒"));
    CHECK(keys.next() == Key::of(Key::Type::Backspace));
    CHECK(keys.next() == Key::of(Key::Type::CtrlD));
    CHECK(keys.next() == Key::of(Key::Type::Eof));
}

TEST_CASE("bare escape decodes when no bytes follow") {
    ScriptedTerminal terminal("\x1b");
    CHECK(terminal.terminal().keys.next() == term::Key::of(term::Key::Type::Esc));
}
