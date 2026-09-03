// Type-to-filter question picker. Filtering happens on every keystroke with
// zero network calls: the whole problem set is already in memory.
#pragma once

#include <string>

#include "lcpush/search.hpp"
#include "lcpush/term/terminal.hpp"

namespace lcpush::picker {

inline constexpr int kResultLimit = 10;
inline constexpr int kMinWidth = 48;

// "1. Two Sum ......... [Easy] 🔒", difficulty right-aligned. Lengths count
// code points, matching the Python len() math.
std::string format_row(const Question& question, int width);

// Run the interactive picker and return the chosen question.
// Throws Cancelled on Ctrl-C or Esc, or when the index is empty.
Question pick(term::Terminal& terminal, const search::ProblemIndex& index,
              const std::string& initial = "");

}  // namespace lcpush::picker
