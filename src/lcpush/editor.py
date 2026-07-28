"""$EDITOR / stdin solution sources (spec §6.1)."""

from __future__ import annotations

import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

SOLUTION_HEADER = "# Paste your solution below. Save and close to continue.\n"
STDIN_SENTINEL = "EOF"


def editor_command() -> list[str]:
    """$VISUAL -> $EDITOR -> vi."""
    raw = os.environ.get("VISUAL") or os.environ.get("EDITOR") or "vi"
    try:
        parts = shlex.split(raw)
    except ValueError:
        parts = [raw]
    return parts or ["vi"]


def strip_header(text: str, header: str) -> str:
    """Remove the instruction header we injected, wherever the user left it."""
    lines = text.replace("\r\n", "\n").split("\n")
    header_lines = {line.strip() for line in header.split("\n") if line.strip()}
    kept = [line for line in lines if line.strip() not in header_lines]
    return "\n".join(kept)


def open_editor(
    *, initial: str = "", header: str = SOLUTION_HEADER, suffix: str = ".txt"
) -> str | None:
    """Open an editor on a temp file; return the content, or None if unchanged/empty."""
    seed = header + initial
    handle = tempfile.NamedTemporaryFile(
        "w", suffix=suffix, delete=False, encoding="utf-8"
    )
    path = Path(handle.name)
    try:
        handle.write(seed)
        handle.close()
        command = [*editor_command(), str(path)]
        try:
            subprocess.run(command, check=False)
        except OSError:
            return None
        result = path.read_text(encoding="utf-8")
    finally:
        try:
            path.unlink()
        except OSError:  # pragma: no cover - best effort cleanup
            pass

    if result == seed:
        return None
    body = strip_header(result, header) if header else result
    return body if body.strip() else None


def read_stdin(stream=None) -> str:
    """Read until EOF (Ctrl-D) or a lone `EOF` sentinel line (spec §6.1)."""
    source = stream if stream is not None else sys.stdin
    lines: list[str] = []
    for line in source:
        if line.rstrip("\r\n") == STDIN_SENTINEL:
            break
        lines.append(line)
    return "".join(lines)
