"""GitHub token resolution and storage (spec §4).

Resolution order, first hit wins:
  1. $LCPUSH_GITHUB_TOKEN
  2. $GITHUB_TOKEN
  3. OS keyring
  4. `gh auth token`
  5. the token fallback file (used when the keyring is unavailable)

The token is never written to config.toml and never echoed back.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass

from .paths import token_file, write_private

SERVICE = "lcpush"
ACCOUNT = "github"

ENV_VARS = ("LCPUSH_GITHUB_TOKEN", "GITHUB_TOKEN")


@dataclass(frozen=True)
class ResolvedToken:
    value: str
    source: str


def _keyring():
    try:
        import keyring  # imported lazily: the backend probe is slow on Linux
    except Exception:  # pragma: no cover - keyring is a hard dependency
        return None
    return keyring


def from_env() -> ResolvedToken | None:
    for name in ENV_VARS:
        value = os.environ.get(name, "").strip()
        if value:
            return ResolvedToken(value, f"${name}")
    return None


def from_keyring() -> ResolvedToken | None:
    keyring = _keyring()
    if keyring is None:
        return None
    try:
        value = keyring.get_password(SERVICE, ACCOUNT)
    except Exception:
        return None
    if value:
        return ResolvedToken(value.strip(), "keyring")
    return None


def from_gh_cli() -> ResolvedToken | None:
    if not shutil.which("gh"):
        return None
    try:
        result = subprocess.run(
            ["gh", "auth", "token"],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if result.returncode != 0:
        return None
    value = result.stdout.strip()
    return ResolvedToken(value, "gh auth token") if value else None


def from_file() -> ResolvedToken | None:
    path = token_file()
    if not path.exists():
        return None
    try:
        value = path.read_text(encoding="utf-8").strip()
    except OSError:
        return None
    return ResolvedToken(value, str(path)) if value else None


def resolve() -> ResolvedToken | None:
    """First hit wins across every source, or None if nothing is stored."""
    for source in (from_env, from_keyring, from_gh_cli, from_file):
        found = source()
        if found is not None:
            return found
    return None


def store(token: str) -> str:
    """Persist a token, preferring the keyring. Returns a description of where.

    Falls back to a 0600 file when no keyring backend is usable; the caller is
    expected to surface that as a warning.
    """
    value = token.strip()
    keyring = _keyring()
    if keyring is not None:
        try:
            keyring.set_password(SERVICE, ACCOUNT, value)
            return "keyring"
        except Exception:
            pass
    path = token_file()
    write_private(path, value + "\n")
    return str(path)


def clear() -> list[str]:
    """Remove every token lcpush itself stored. Returns what was cleared."""
    cleared: list[str] = []
    keyring = _keyring()
    if keyring is not None:
        try:
            if keyring.get_password(SERVICE, ACCOUNT):
                keyring.delete_password(SERVICE, ACCOUNT)
                cleared.append("keyring")
        except Exception:
            pass
    path = token_file()
    if path.exists():
        try:
            path.unlink()
            cleared.append(str(path))
        except OSError:
            pass
    return cleared


def redact(text: str, token: str | None) -> str:
    """Scrub a token out of any string headed for a terminal or a log."""
    if not token or len(token) < 8:
        return text
    return text.replace(token, "****")
