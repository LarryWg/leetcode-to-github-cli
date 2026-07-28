"""Clipboard reading across platforms (spec §6.1).

A missing tool or an empty clipboard is not an error — the caller simply drops
the Clipboard option from the source menu.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys

TIMEOUT_SECONDS = 5


def clipboard_command() -> list[str] | None:
    """The paste command for this platform, or None if nothing is available."""
    if sys.platform == "darwin":
        if shutil.which("pbpaste"):
            return ["pbpaste"]
        return None
    if sys.platform == "win32":  # pragma: no cover - exercised on Windows only
        if shutil.which("powershell"):
            return ["powershell", "-NoProfile", "-Command", "Get-Clipboard"]
        return None
    if os.environ.get("WAYLAND_DISPLAY") and shutil.which("wl-paste"):
        return ["wl-paste", "--no-newline"]
    if shutil.which("xclip"):
        return ["xclip", "-selection", "clipboard", "-o"]
    if shutil.which("xsel"):
        return ["xsel", "-b"]
    if shutil.which("wl-paste"):
        return ["wl-paste", "--no-newline"]
    return None


def available() -> bool:
    return clipboard_command() is not None


def read() -> str | None:
    """Clipboard contents, or None when unreadable, empty, or whitespace-only."""
    command = clipboard_command()
    if command is None:
        return None
    try:
        result = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=TIMEOUT_SECONDS,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if result.returncode != 0:
        return None
    content = result.stdout
    return content if content and content.strip() else None
