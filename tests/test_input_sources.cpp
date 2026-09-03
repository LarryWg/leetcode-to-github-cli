#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

#include "helpers/fake_subprocess.hpp"
#include "helpers/fixtures.hpp"
#include "lcpush/clipboard.hpp"
#include "lcpush/editor.hpp"

using namespace lcpush;
using lcpush::testing::EnvVar;
using lcpush::testing::FakeSubprocess;

TEST_CASE("read stdin until eof sentinel") {
    std::istringstream stream("line one\nline two\nEOF\nignored\n");
    CHECK(editor::read_stdin(stream) == "line one\nline two\n");
}

TEST_CASE("read stdin until end of stream") {
    std::istringstream stream("a\nb\n");
    CHECK(editor::read_stdin(stream) == "a\nb\n");
}

TEST_CASE("editor command prefers visual") {
    EnvVar visual("VISUAL", "code --wait");
    EnvVar editor_var("EDITOR", "nano");
    CHECK(editor::editor_command() == std::vector<std::string>{"code", "--wait"});
}

TEST_CASE("editor command falls back to vi") {
    EnvVar visual("VISUAL", std::nullopt);
    EnvVar editor_var("EDITOR", std::nullopt);
    CHECK(editor::editor_command() == std::vector<std::string>{"vi"});
}

TEST_CASE("strip header removes the instruction line") {
    std::string body = std::string(editor::kSolutionHeader) + "print(1)\n";
    auto stripped = editor::strip_header(body, editor::kSolutionHeader);
    CHECK(stripped.find("print(1)") != std::string::npos);
    CHECK(stripped.find("Paste your solution") == std::string::npos);
}

TEST_CASE("open editor returns none when unchanged") {
    EnvVar editor_var("EDITOR", "true");
    FakeSubprocess proc;
    proc.on_run_interactive = [](const std::vector<std::string>&) {
        return std::optional<int>{0};
    };
    util::SubprocessOverride subprocess_override(&proc);
    CHECK_FALSE(editor::open_editor().has_value());
}

TEST_CASE("open editor returns written content") {
    EnvVar editor_var("EDITOR", "fake");
    FakeSubprocess proc;
    proc.on_run_interactive = [](const std::vector<std::string>& command) {
        std::ofstream out(command.back());
        out << editor::kSolutionHeader << "class Solution: pass\n";
        return std::optional<int>{0};
    };
    util::SubprocessOverride subprocess_override(&proc);
    auto result = editor::open_editor();
    REQUIRE(result.has_value());
    CHECK(result->find("class Solution: pass") != std::string::npos);
    CHECK(result->find("Paste your solution") == std::string::npos);
}

TEST_CASE("open editor returns none for empty buffer") {
    EnvVar editor_var("EDITOR", "fake");
    FakeSubprocess proc;
    proc.on_run_interactive = [](const std::vector<std::string>& command) {
        std::ofstream out(command.back());
        out << "\n\n";
        return std::optional<int>{0};
    };
    util::SubprocessOverride subprocess_override(&proc);
    CHECK_FALSE(editor::open_editor().has_value());
}

TEST_CASE("clipboard command on macos") {
    FakeSubprocess proc;
    proc.on_which = [](const std::string&) {
        return std::optional<std::string>{"/usr/bin/pbpaste"};
    };
    util::SubprocessOverride subprocess_override(&proc);
    auto command = clipboard::clipboard_command("darwin");
    REQUIRE(command.has_value());
    CHECK(*command == std::vector<std::string>{"pbpaste"});
}

TEST_CASE("clipboard command missing tool") {
    EnvVar wayland("WAYLAND_DISPLAY", std::nullopt);
    FakeSubprocess proc;
    util::SubprocessOverride subprocess_override(&proc);
    CHECK_FALSE(clipboard::clipboard_command("linux").has_value());
}

TEST_CASE("clipboard prefers wayland") {
    EnvVar wayland("WAYLAND_DISPLAY", "wayland-0");
    FakeSubprocess proc;
    proc.on_which = [](const std::string& name) {
        return std::optional<std::string>{"/usr/bin/" + name};
    };
    util::SubprocessOverride subprocess_override(&proc);
    auto command = clipboard::clipboard_command("linux");
    REQUIRE(command.has_value());
    CHECK((*command)[0] == "wl-paste");
}

TEST_CASE("clipboard read returns none when whitespace") {
    FakeSubprocess proc;
    proc.on_which = [](const std::string& name) {
        return std::optional<std::string>{"/usr/bin/" + name};
    };
    proc.on_run_capture = [](const std::vector<std::string>&) {
        return util::RunResult{0, "   \n", true};
    };
    util::SubprocessOverride subprocess_override(&proc);
    CHECK_FALSE(clipboard::read().has_value());
}

TEST_CASE("clipboard read returns content") {
    FakeSubprocess proc;
    proc.on_which = [](const std::string& name) {
        return std::optional<std::string>{"/usr/bin/" + name};
    };
    proc.on_run_capture = [](const std::vector<std::string>&) {
        return util::RunResult{0, "class Solution: pass\n", true};
    };
    util::SubprocessOverride subprocess_override(&proc);
    auto content = clipboard::read();
    REQUIRE(content.has_value());
    CHECK(*content == "class Solution: pass\n");
}

TEST_CASE("clipboard read survives a failing tool") {
    FakeSubprocess proc;
    proc.on_which = [](const std::string& name) {
        return std::optional<std::string>{"/usr/bin/" + name};
    };
    proc.on_run_capture = [](const std::vector<std::string>&) {
        return util::RunResult{};  // spawn failure
    };
    util::SubprocessOverride subprocess_override(&proc);
    CHECK_FALSE(clipboard::read().has_value());
}
