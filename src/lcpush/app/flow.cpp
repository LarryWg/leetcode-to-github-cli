#include "lcpush/app/flow.hpp"

#include <unistd.h>

#include <iostream>

#include "lcpush/platform/clipboard.hpp"
#include "lcpush/ui/picker.hpp"
#include "lcpush/term/terminal.hpp"

namespace lcpush::flow {

Deps default_deps() {
    Deps deps;
    deps.stdin_isatty = [] { return ::isatty(STDIN_FILENO) != 0; };
    deps.select = [](const std::string& message, const std::vector<prompts::Choice>& choices,
                     const std::optional<std::string>& default_value) {
        return prompts::select(term::real_terminal(), message, choices, default_value);
    };
    deps.confirm = [](const std::string& message, bool default_value) {
        return prompts::confirm(term::real_terminal(), message, default_value);
    };
    deps.read_key = [](const std::map<std::string, std::string>& allowed) {
        return prompts::read_key(term::real_terminal(), allowed);
    };
    deps.edit_line = [](const std::string& message, const std::string& default_value) {
        return prompts::edit_line(term::real_terminal(), message, default_value);
    };
    deps.text = [](const std::string& message, const std::string& default_value) {
        return prompts::text(term::real_terminal(), message, default_value);
    };
    deps.password = [](const std::string& message) {
        return prompts::password(term::real_terminal(), message);
    };
    deps.clipboard_read = [] { return clipboard::read(); };
    deps.open_editor = [](const editor::EditorOptions& options) {
        return editor::open_editor(options);
    };
    deps.read_stdin = [] { return editor::read_stdin(std::cin); };
    deps.pick = [](const search::ProblemIndex& index) {
        return picker::pick(term::real_terminal(), index);
    };
    deps.github = [](const std::string& token) -> std::unique_ptr<github::Api> {
        return std::make_unique<github::GitHubClient>(token);
    };
    return deps;
}

}  // namespace lcpush::flow
