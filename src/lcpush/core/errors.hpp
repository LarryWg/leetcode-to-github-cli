// Exception hierarchy mirroring the original Python errors module.
// The CLI entry point is the only place that turns these into output.
#pragma once

#include <stdexcept>
#include <string>

namespace lcpush {

// Base for everything lcpush raises on purpose. Carries the exit code.
struct LcpushError : std::runtime_error {
    int exit_code;

    explicit LcpushError(std::string message, int code = 1)
        : std::runtime_error(std::move(message)), exit_code(code) {}

    const char* message() const noexcept { return what(); }
};

// Config file missing, unreadable, or invalid.
struct ConfigError : LcpushError {
    using LcpushError::LcpushError;
};

// No usable GitHub token, or the token lacks access.
struct TokenError : LcpushError {
    using LcpushError::LcpushError;
};

// GitHub API failures other than auth.
struct GitHubError : LcpushError {
    using LcpushError::LcpushError;
};

// LeetCode problem list could not be fetched.
struct LeetCodeError : LcpushError {
    using LcpushError::LcpushError;
};

// Solution content was rejected or no terminal is available.
struct InputError : LcpushError {
    using LcpushError::LcpushError;
};

// User backed out. Exit code 130 by convention (SIGINT).
struct Cancelled : LcpushError {
    explicit Cancelled(std::string message = "Aborted.")
        : LcpushError(std::move(message), 130) {}
};

}  // namespace lcpush
