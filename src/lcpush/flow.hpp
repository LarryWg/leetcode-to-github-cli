// Injectable dependencies for the interactive flow. Production uses
// default_deps(), session/onboarding/cli tests script individual entries the
// way the Python tests monkeypatched module attributes.
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lcpush/editor.hpp"
#include "lcpush/github.hpp"
#include "lcpush/http/transport.hpp"
#include "lcpush/problems.hpp"
#include "lcpush/prompts.hpp"
#include "lcpush/search.hpp"

namespace lcpush::flow {

struct Deps {
    std::function<bool()> stdin_isatty;

    std::function<std::string(const std::string& message,
                              const std::vector<prompts::Choice>& choices,
                              const std::optional<std::string>& default_value)>
        select;
    std::function<bool(const std::string& message, bool default_value)> confirm;
    std::function<std::string(const std::map<std::string, std::string>& allowed)> read_key;
    std::function<std::string(const std::string& message, const std::string& default_value)>
        edit_line;
    std::function<std::string(const std::string& message, const std::string& default_value)>
        text;
    std::function<std::string(const std::string& message)> password;

    std::function<std::optional<std::string>()> clipboard_read;
    std::function<std::optional<std::string>(const editor::EditorOptions& options)>
        open_editor;
    std::function<std::string()> read_stdin;
    std::function<Question(const search::ProblemIndex& index)> pick;

    std::function<std::unique_ptr<github::Api>(const std::string& token)> github;
    // Empty means the default curl transport.
    http::TransportFactory problems_transport;
};

Deps default_deps();

}  // namespace lcpush::flow
