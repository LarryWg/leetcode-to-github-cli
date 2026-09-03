// Subprocess seam. Production uses fork/exec, tests install an override so
// clipboard/gh/editor behavior can be scripted without spawning anything.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace lcpush::util {

struct RunResult {
    int exit_code = -1;
    std::string out;
    bool ok = false;  // false on spawn failure or timeout
};

class Subprocess {
  public:
    virtual ~Subprocess() = default;

    // Run capturing stdout, killing the child after timeout_seconds.
    virtual RunResult run_capture(const std::vector<std::string>& argv,
                                  int timeout_seconds) = 0;

    // Run inheriting the terminal, waiting until exit. Returns the exit code
    // or nullopt when the command could not be spawned.
    virtual std::optional<int> run_interactive(const std::vector<std::string>& argv) = 0;

    // PATH lookup like shutil.which.
    virtual std::optional<std::string> which(const std::string& name) = 0;
};

// The active implementation: the test override when set, else the real one.
Subprocess& subprocess();

// Install an override for tests, nullptr restores the real implementation.
void set_subprocess_override(Subprocess* override_impl);

// RAII helper for tests.
class SubprocessOverride {
  public:
    explicit SubprocessOverride(Subprocess* impl) { set_subprocess_override(impl); }
    ~SubprocessOverride() { set_subprocess_override(nullptr); }
    SubprocessOverride(const SubprocessOverride&) = delete;
    SubprocessOverride& operator=(const SubprocessOverride&) = delete;
};

}  // namespace lcpush::util
