// CLI entry point: flags, config subcommands, top-level error handling.
#pragma once

#include <string>
#include <vector>

#include "lcpush/flow.hpp"

namespace lcpush::cli {

// Parse args (without argv[0]) and run. Returns the process exit code.
int run(const std::vector<std::string>& args, const flow::Deps& deps);

}  // namespace lcpush::cli
