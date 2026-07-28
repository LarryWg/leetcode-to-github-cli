"""XDG-correct config and cache locations (spec §4).

`$XDG_CONFIG_HOME/lcpush/` defaulting to `~/.config/lcpush/`, and
`$XDG_CACHE_HOME/lcpush/` defaulting to `~/.cache/lcpush/`. The
`LCPUSH_CONFIG_DIR` / `LCPUSH_CACHE_DIR` overrides exist so tests (and users
with unusual setups) can relocate state without touching XDG globals.
"""

from __future__ import annotations

import os
import stat
import sys
from pathlib import Path

import platformdirs

APP_NAME = "lcpush"


def config_dir() -> Path:
    """Directory holding `config.toml` (and the token fallback file)."""
    override = os.environ.get("LCPUSH_CONFIG_DIR")
    if override:
        return Path(override).expanduser()
    xdg = os.environ.get("XDG_CONFIG_HOME")
    if xdg:
        return Path(xdg).expanduser() / APP_NAME
    if sys.platform == "win32":
        return Path(platformdirs.user_config_dir(APP_NAME, appauthor=False))
    return Path.home() / ".config" / APP_NAME


def cache_dir() -> Path:
    """Directory holding `problems.json`."""
    override = os.environ.get("LCPUSH_CACHE_DIR")
    if override:
        return Path(override).expanduser()
    xdg = os.environ.get("XDG_CACHE_HOME")
    if xdg:
        return Path(xdg).expanduser() / APP_NAME
    if sys.platform == "win32":
        return Path(platformdirs.user_cache_dir(APP_NAME, appauthor=False))
    return Path.home() / ".cache" / APP_NAME


def config_file() -> Path:
    return config_dir() / "config.toml"


def token_file() -> Path:
    """Fallback token location, used only when the keyring is unavailable."""
    return config_dir() / "token"


def problems_cache_file() -> Path:
    return cache_dir() / "problems.json"


def write_private(path: Path, text: str) -> None:
    """Write `text` to `path` with mode 0600, creating parents as needed."""
    path.parent.mkdir(parents=True, exist_ok=True)
    # Create with restrictive permissions before any bytes hit the disk.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            handle.write(text)
    finally:
        try:
            os.chmod(path, stat.S_IRUSR | stat.S_IWUSR)
        except OSError:  # pragma: no cover - platforms without POSIX modes
            pass
