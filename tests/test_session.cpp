// The interactive flow, driven by scripted prompt answers.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <deque>
#include <memory>

#include "helpers/capture.hpp"
#include "helpers/fixtures.hpp"
#include "lcpush/config/config.hpp"
#include "lcpush/core/errors.hpp"
#include "lcpush/platform/paths.hpp"
#include "lcpush/leetcode/problems.hpp"
#include "lcpush/app/session.hpp"

using namespace lcpush;
using lcpush::testing::CaptureStreams;
using lcpush::testing::EnvVar;
using lcpush::testing::IsolatedDirs;
using lcpush::testing::kCppSolution;
using lcpush::testing::kPySolution;

namespace {

constexpr const char* kUrlOnClipboard = "https://leetcode.com/problems/two-sum/";

struct PutCall {
    std::string path;
    github::PutFileOptions options;
};

// Shared FakeGitHub state, one per fixture.
struct GitHubState {
    std::vector<PutCall> puts;
    std::optional<std::string> existing_sha;
};

class FakeGitHub final : public github::Api {
  public:
    explicit FakeGitHub(GitHubState& state) : state_(state) {}

    github::RepoInfo get_repo(const std::string& owner, const std::string& name) override {
        return {owner + "/" + name, "main", true};
    }

    std::optional<std::string> get_file_sha(const std::string&, const std::string&,
                                            const std::string&, const std::string&) override {
        return state_.existing_sha;
    }

    github::PushResult put_file(const std::string& owner, const std::string& name,
                                const std::string& path,
                                const github::PutFileOptions& options) override {
        state_.puts.push_back({path, options});
        return {"https://github.com/" + owner + "/" + name + "/blob/main/" + path, "sha",
                false};
    }

  private:
    GitHubState& state_;
};

struct SelectCall {
    std::string message;
    std::vector<prompts::Choice> entries;
    std::optional<std::string> default_value;
};

// Scripted answers for every interactive prompt, plus a call log.
struct Recorder {
    std::deque<std::string> keys;
    std::deque<bool> confirms;
    std::map<std::string, std::string> selects;
    std::deque<std::string> lines;
    std::vector<SelectCall> select_calls;
    std::vector<std::string> edit_defaults;

    std::string select(const std::string& message,
                       const std::vector<prompts::Choice>& entries,
                       const std::optional<std::string>& default_value) {
        select_calls.push_back({message, entries, default_value});
        auto scripted = selects.find(message);
        if (scripted != selects.end()) return scripted->second;
        if (default_value) return *default_value;
        return entries[0].value;
    }

    bool confirm(const std::string&, bool default_value) {
        if (confirms.empty()) return default_value;
        bool answer = confirms.front();
        confirms.pop_front();
        return answer;
    }

    std::string read_key(const std::map<std::string, std::string>&) {
        if (keys.empty()) return "push";
        std::string key = keys.front();
        keys.pop_front();
        return key;
    }

    std::string edit_line(const std::string&, const std::string& default_value) {
        edit_defaults.push_back(default_value);
        if (lines.empty()) return default_value;
        std::string line = lines.front();
        lines.pop_front();
        return line;
    }
};

// The interactive fixture: configured repo, warm cache, env token, fakes.
struct Interactive {
    IsolatedDirs dirs;
    EnvVar token_env{"LCPUSH_GITHUB_TOKEN", "ghp_test_token"};
    CaptureStreams capture;
    GitHubState github;
    Recorder recorder;
    flow::Deps deps;

    Interactive() {
        config::save(config::set_value(config::Config{}, "repo", "user/leetcode-solutions"));
        problems::save_cache(testing::questions());

        deps.stdin_isatty = [] { return true; };
        deps.pick = [](const search::ProblemIndex&) { return testing::questions()[0]; };
        deps.clipboard_read = [] { return std::optional<std::string>{kPySolution}; };
        deps.github = [this](const std::string&) -> std::unique_ptr<github::Api> {
            return std::make_unique<FakeGitHub>(github);
        };
        deps.select = [this](const std::string& message,
                             const std::vector<prompts::Choice>& entries,
                             const std::optional<std::string>& default_value) {
            return recorder.select(message, entries, default_value);
        };
        deps.confirm = [this](const std::string& message, bool default_value) {
            return recorder.confirm(message, default_value);
        };
        deps.read_key = [this](const std::map<std::string, std::string>& allowed) {
            return recorder.read_key(allowed);
        };
        deps.edit_line = [this](const std::string& message, const std::string& value) {
            return recorder.edit_line(message, value);
        };
    }
};

}  // namespace

TEST_CASE("happy path clipboard to push") {
    Interactive fixture;
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"push"};
    CHECK(session::run(fixture.deps) == 0);

    REQUIRE(fixture.github.puts.size() == 1);
    const PutCall& put = fixture.github.puts[0];
    CHECK(put.path == "0001-two-sum.py");
    CHECK(put.options.content == kPySolution);
    CHECK(put.options.message == "Add 1. Two Sum (Python3)");

    std::string out = fixture.capture.out();
    CHECK(out.find("┌ Clipboard — 8 lines") != std::string::npos);
    CHECK(out.find("┌ Ready to push") != std::string::npos);
    CHECK(out.find("Message  Add 1. Two Sum (Python3)") != std::string::npos);
    CHECK(out.find("blob/main/0001-two-sum.py") != std::string::npos);
    CHECK(fixture.recorder.edit_defaults.empty());
}

TEST_CASE("piped stdin is refused") {
    Interactive fixture;
    fixture.deps.stdin_isatty = [] { return false; };
    CHECK_THROWS_MATCHES(
        session::run(fixture.deps), InputError,
        Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("terminal")));
}

TEST_CASE("clipboard is preselected when plausible") {
    Interactive fixture;
    fixture.recorder.confirms = {true};
    session::run(fixture.deps);
    REQUIRE_FALSE(fixture.recorder.select_calls.empty());
    const SelectCall& call = fixture.recorder.select_calls[0];
    CHECK(call.message == "? Solution source:");
    CHECK(call.default_value == "clipboard");
    CHECK(call.entries[0].title == "Clipboard");
}

TEST_CASE("url on clipboard is labelled and not preselected") {
    Interactive fixture;
    fixture.deps.clipboard_read = [] { return std::optional<std::string>{kUrlOnClipboard}; };
    fixture.deps.open_editor = [](const editor::EditorOptions&) {
        return std::optional<std::string>{kPySolution};
    };
    fixture.recorder.confirms = {true};
    session::run(fixture.deps);

    const SelectCall& call = fixture.recorder.select_calls[0];
    CHECK(call.default_value == "editor");
    CHECK(call.entries[0].title == "Clipboard (doesn't look like code)");
    CHECK(call.entries[0].value == "clipboard");
}

TEST_CASE("declining the preview returns to the source menu") {
    Interactive fixture;
    fixture.deps.open_editor = [](const editor::EditorOptions&) {
        return std::optional<std::string>{kPySolution};
    };
    fixture.recorder.confirms = {false, true};
    // First pass takes the clipboard and is declined; second pass picks Editor.
    auto answers = std::make_shared<std::deque<std::string>>(
        std::deque<std::string>{"clipboard", "editor"});
    auto& recorder = fixture.recorder;
    fixture.deps.select = [answers, &recorder](
                              const std::string& message,
                              const std::vector<prompts::Choice>& entries,
                              const std::optional<std::string>& default_value) {
        std::string result = recorder.select(message, entries, default_value);
        if (message == "? Solution source:" && !answers->empty()) {
            result = answers->front();
            answers->pop_front();
        }
        return result;
    };

    CHECK(session::run(fixture.deps) == 0);
    CHECK(fixture.github.puts.size() == 1);
    int source_menus = 0;
    for (const SelectCall& call : fixture.recorder.select_calls) {
        if (call.message == "? Solution source:") ++source_menus;
    }
    CHECK(source_menus == 2);
}

TEST_CASE("pressing m prefills the rendered message") {
    Interactive fixture;
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"edit", "push"};
    fixture.recorder.lines = {"Add 1. Two Sum (Python3) via hash map"};
    session::run(fixture.deps);
    CHECK(fixture.recorder.edit_defaults ==
          std::vector<std::string>{"Add 1. Two Sum (Python3)"});
    CHECK(fixture.github.puts[0].options.message == "Add 1. Two Sum (Python3) via hash map");
}

TEST_CASE("empty message is rejected and reprompted") {
    Interactive fixture;
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"edit", "push"};
    fixture.recorder.lines = {"   ", "Real message"};
    session::run(fixture.deps);
    CHECK(fixture.recorder.edit_defaults.size() == 2);
    CHECK(fixture.github.puts[0].options.message == "Real message");
}

TEST_CASE("n cancels without pushing") {
    Interactive fixture;
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"cancel"};
    try {
        session::run(fixture.deps);
        FAIL("expected Cancelled");
    } catch (const Cancelled& exc) {
        CHECK(exc.exit_code == 130);
    }
    CHECK(fixture.github.puts.empty());
}

TEST_CASE("overwrite is confirmed by the panel alone") {
    Interactive fixture;
    fixture.github.existing_sha = "existing";
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"push"};
    session::run(fixture.deps);

    std::string out = fixture.capture.out();
    CHECK(out.find("┌ Ready to push (overwrites existing file)") != std::string::npos);
    CHECK(out.find("Message  Update 1. Two Sum (Python3)") != std::string::npos);
    REQUIRE(fixture.github.puts[0].options.sha.has_value());
    CHECK(*fixture.github.puts[0].options.sha == "existing");
    CHECK(fixture.recorder.confirms.empty());
}

TEST_CASE("commit prompt never mode hides the edit keys") {
    Interactive fixture;
    config::save(config::set_value(
        config::set_value(config::Config{}, "repo", "user/leetcode-solutions"),
        "commit.prompt", "never"));
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"push"};
    session::run(fixture.deps);
    CHECK(fixture.github.puts[0].options.message == "Add 1. Two Sum (Python3)");
    CHECK(fixture.recorder.edit_defaults.empty());
    CHECK(fixture.capture.out().find("[m] edit message") == std::string::npos);
}

TEST_CASE("commit prompt always opens the message field") {
    Interactive fixture;
    config::save(config::set_value(
        config::set_value(config::Config{}, "repo", "user/leetcode-solutions"),
        "commit.prompt", "always"));
    fixture.recorder.confirms = {true};
    fixture.recorder.keys = {"push"};
    fixture.recorder.lines = {"Edited up front"};
    session::run(fixture.deps);
    CHECK(fixture.recorder.edit_defaults ==
          std::vector<std::string>{"Add 1. Two Sum (Python3)"});
    CHECK(fixture.github.puts[0].options.message == "Edited up front");
}

TEST_CASE("language menu preselects the detection") {
    Interactive fixture;
    fixture.recorder.confirms = {true};
    session::run(fixture.deps);
    for (const SelectCall& call : fixture.recorder.select_calls) {
        if (call.message != "? Language:") continue;
        CHECK(call.default_value == "python3");
        CHECK(call.entries[0].title == "Python3  (detected)");
        CHECK(call.entries.size() == 14);
        return;
    }
    FAIL("language menu never shown");
}

TEST_CASE("undetectable language preselects nothing") {
    Interactive fixture;
    fixture.deps.clipboard_read = [] {
        return std::optional<std::string>{"the quick brown fox jumps\n"};
    };
    fixture.recorder.confirms = {true};
    fixture.recorder.selects = {{"? Solution source:", "clipboard"},
                                {"? Language:", "python3"}};
    session::run(fixture.deps);
    for (const SelectCall& call : fixture.recorder.select_calls) {
        if (call.message != "? Language:") continue;
        CHECK_FALSE(call.default_value.has_value());
        return;
    }
    FAIL("language menu never shown");
}

TEST_CASE("empty editor buffer aborts") {
    Interactive fixture;
    fixture.deps.open_editor = [](const editor::EditorOptions&) {
        return std::optional<std::string>{};
    };
    fixture.recorder.selects = {{"? Solution source:", "editor"}};
    CHECK_THROWS_MATCHES(session::run(fixture.deps), InputError,
                         Catch::Matchers::Message("No solution provided. Aborted."));
    CHECK(fixture.github.puts.empty());
}

TEST_CASE("empty clipboard at read time returns to the menu") {
    Interactive fixture;
    auto reads = std::make_shared<int>(0);
    fixture.deps.clipboard_read = [reads]() -> std::optional<std::string> {
        ++*reads;
        if (*reads > 2) return std::string(kPySolution);
        return std::nullopt;
    };
    fixture.deps.open_editor = [](const editor::EditorOptions&) {
        return std::optional<std::string>{kPySolution};
    };
    fixture.recorder.confirms = {true};
    fixture.recorder.selects = {{"? Solution source:", "clipboard"}};
    session::run(fixture.deps);
    int source_menus = 0;
    for (const SelectCall& call : fixture.recorder.select_calls) {
        if (call.message == "? Solution source:") ++source_menus;
    }
    CHECK(source_menus >= 2);
    CHECK(fixture.github.puts[0].options.content == kPySolution);
}

TEST_CASE("truncated paste warns and shows the tail") {
    Interactive fixture;
    std::string full = kCppSolution;
    // First 8 lines of the C++ solution, like the Python test.
    size_t offset = 0;
    for (int i = 0; i < 8; ++i) offset = full.find('\n', offset) + 1;
    std::string truncated = full.substr(0, offset);
    fixture.deps.clipboard_read = [truncated] {
        return std::optional<std::string>{truncated};
    };
    fixture.recorder.confirms = {true};
    session::run(fixture.deps);
    CHECK(fixture.capture.err().find("Brackets are unbalanced") != std::string::npos);
    CHECK(fixture.capture.out().find("8 lines") != std::string::npos);
    CHECK(fixture.capture.out().find("seen[nums[i]] = i;") != std::string::npos);
}

TEST_CASE("token source is announced on first run") {
    Interactive fixture;
    std::filesystem::remove(paths::config_file());
    fixture.deps.text = [](const std::string&, const std::string&) { return "o/r"; };
    fixture.recorder.confirms = {true};
    CHECK(session::run(fixture.deps) == 0);
    std::string out = fixture.capture.out();
    CHECK(out.find("Using GitHub token from $LCPUSH_GITHUB_TOKEN") != std::string::npos);
    CHECK(out.find("ghp_test_token") == std::string::npos);
}

TEST_CASE("wrong question warning is not blocking") {
    Interactive fixture;
    fixture.deps.clipboard_read = [] {
        return std::optional<std::string>{
            "var lengthOfLongestSubstring = function(s) {\n    return 0;\n};\n"};
    };
    fixture.recorder.confirms = {true};
    CHECK(session::run(fixture.deps) == 0);
    CHECK(fixture.capture.err().find("but you selected \"Two Sum\"") != std::string::npos);
}
