"""Typed errors.

Every failure path in lcpush raises one of these. The CLI catches them at the
top level and prints a single line plus a non-zero exit code — never a
traceback (spec §9).
"""

from __future__ import annotations


class LcpushError(Exception):
    """Base error: one clear message, one exit code."""

    exit_code = 1

    def __init__(self, message: str, *, exit_code: int | None = None) -> None:
        super().__init__(message)
        self.message = message
        if exit_code is not None:
            self.exit_code = exit_code


class ConfigError(LcpushError):
    """Malformed or missing configuration."""


class TokenError(LcpushError):
    """Token missing, invalid, or lacking the required scope."""


class GitHubError(LcpushError):
    """The GitHub API refused or failed the request."""


class LeetCodeError(LcpushError):
    """The problem set could not be reached and no cache exists."""


class InputError(LcpushError):
    """The user supplied no usable solution content."""


class Cancelled(LcpushError):
    """The user aborted (Ctrl-C or an explicit cancel)."""

    exit_code = 130

    def __init__(self, message: str = "Aborted.") -> None:
        super().__init__(message)
