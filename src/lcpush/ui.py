"""Terminal rendering: colors, preview box, ready-to-push panel (spec §6.3, §8.3).

The panel builders are pure string functions so they can be asserted in tests
without a terminal.
"""

from __future__ import annotations

import os
import sys

from .solution import byte_size, line_count

RESET = "\033[0m"
DIM = "\033[2m"
BOLD = "\033[1m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
CYAN = "\033[36m"

HEAD_LINES = 5
TAIL_LINES = 3


def color_enabled(stream=None) -> bool:
    target = stream or sys.stdout
    if os.environ.get("NO_COLOR"):
        return False
    if os.environ.get("LCPUSH_FORCE_COLOR"):
        return True
    return bool(getattr(target, "isatty", lambda: False)())


def paint(text: str, code: str, *, stream=None) -> str:
    return f"{code}{text}{RESET}" if color_enabled(stream) else text


def success(message: str) -> None:
    print(paint(f"✓ {message}", GREEN))


def info(message: str) -> None:
    print(message)


def dim(message: str) -> None:
    print(paint(message, DIM))


def warn(message: str) -> None:
    print(paint(message, YELLOW), file=sys.stderr)


def error(message: str) -> None:
    print(paint(f"✗ {message}", RED), file=sys.stderr)


def arrow(message: str) -> None:
    print(paint(f"→ {message}", CYAN))


def preview_lines(text: str) -> list[str]:
    """First 5 and last 3 lines, with an explicit hidden-count marker between.

    Showing only the head hides truncated pastes, which are the more common
    failure (spec §6.3).
    """
    lines = text.rstrip("\n").split("\n")
    if len(lines) <= HEAD_LINES + TAIL_LINES + 1:
        return list(lines)
    hidden = len(lines) - HEAD_LINES - TAIL_LINES
    return [
        *lines[:HEAD_LINES],
        f" … {hidden} lines hidden …",
        *lines[-TAIL_LINES:],
    ]


def render_preview(source_label: str, text: str, language_label: str) -> str:
    """The bordered preview box, without the trailing confirm prompt."""
    header = (
        f"┌ {source_label} — {line_count(text)} lines, "
        f"{byte_size(text)} bytes, detected {language_label}"
    )
    rows = [f"  {header}"]
    rows.extend(f"  │ {line}" for line in preview_lines(text))
    return "\n".join(rows)


def render_push_panel(
    *,
    filename: str,
    lines: int,
    repo: str,
    branch: str,
    message: str,
    updating: bool,
    prompt_mode: str = "confirm",
) -> str:
    """The ready-to-push panel, including its key hints."""
    title = "Ready to push (overwrites existing file)" if updating else "Ready to push"
    subject, _, body = message.partition("\n")
    rows = [
        f"  ┌ {title}",
        f"  │ File     {filename}  ({lines} lines)",
        f"  │ Repo     {repo}  ({branch})",
        f"  │ Message  {subject}",
    ]
    for extra in [line for line in body.split("\n") if body]:
        rows.append(f"  │          {extra}")
    if prompt_mode == "never":
        rows.append("  └ ? [Enter] push   [n] cancel")
    else:
        rows.append(
            "  └ ? [Enter] push   [m] edit message   [M] edit in $EDITOR   [n] cancel"
        )
    return "\n".join(rows)
