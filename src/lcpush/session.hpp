// The lcpush run flow: pick a question, read a solution, push it.
#pragma once

#include "lcpush/flow.hpp"

namespace lcpush::session {

// Entry point for a full push. Returns a process exit code.
int run(const flow::Deps& deps, bool refresh = false);

}  // namespace lcpush::session
