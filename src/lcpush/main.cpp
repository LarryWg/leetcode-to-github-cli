#include <csignal>
#include <string>
#include <unistd.h>
#include <vector>

#include "lcpush/app/cli.hpp"
#include "lcpush/app/flow.hpp"

namespace {

// Prompts run with ISIG off, so this only fires outside raw mode (network
// waits, editor hand-offs). Mirror the Python KeyboardInterrupt handling:
// one line, exit 130. Raw mode is not active here, nothing needs restoring.
void on_sigint(int) {
    const char message[] = "\xe2\x9c\x97 Aborted.\n";
    ::write(STDERR_FILENO, message, sizeof(message) - 1);
    ::_exit(130);
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, on_sigint);
    std::vector<std::string> args(argv + 1, argv + argc);
    return lcpush::cli::run(args, lcpush::flow::default_deps());
}
