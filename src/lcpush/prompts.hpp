// Interactive prompt widgets. Every cancellation (Ctrl-C, Ctrl-D, Esc where
// bound, end of input) becomes Cancelled exactly once, here.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lcpush/term/terminal.hpp"

namespace lcpush::prompts {

struct Choice {
    std::string title;
    std::string value;
};

// Single-select menu. Returns the chosen value; default_value preselects.
std::string select(term::Terminal& terminal, const std::string& message,
                   const std::vector<Choice>& choices,
                   const std::optional<std::string>& default_value = std::nullopt);

// Yes/no. y and n answer immediately, Enter takes the default.
bool confirm(term::Terminal& terminal, const std::string& message, bool default_value = true);

// Free text with the default pre-filled and the cursor at the end.
std::string text(term::Terminal& terminal, const std::string& message,
                 const std::string& default_value = "");

// Hidden input, echoed as asterisks.
std::string password(term::Terminal& terminal, const std::string& message);

// Pre-filled single-line editor: accepting is one keypress, Ctrl-U clears.
std::string edit_line(term::Terminal& terminal, const std::string& message,
                      const std::string& default_value);

// Block for a single keypress. `allowed` maps key names ("enter", "escape",
// or a literal character) to results. Unknown keys are ignored.
std::string read_key(term::Terminal& terminal,
                     const std::map<std::string, std::string>& allowed);

}  // namespace lcpush::prompts
