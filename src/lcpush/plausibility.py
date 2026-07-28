"""Clipboard plausibility scoring (spec §6.2).

This decides *menu ordering only*. It never blocks a choice, and the clipboard
stays selectable however badly it scores.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from .detect import detect
from .solution import LARGE_CLIPBOARD_BYTES, byte_size

PLAUSIBLE_THRESHOLD = 3

ENTRY_SHAPES = re.compile(
    r"class\s+Solution|impl\s+Solution|object\s+Solution|public\s+class\s+Solution"
    r"|var\s+\w+\s*=\s*function\s*\(|func\s+\w+\s*\(|def\s+\w+\s*\(|fun\s+\w+\s*\(",
    re.MULTILINE,
)

BARE_URL = re.compile(r"^\s*(https?://|www\.)\S+\s*$", re.IGNORECASE)
BARE_EMAIL = re.compile(r"^\s*[\w.+-]+@[\w-]+\.[\w.]+\s*$")
BARE_PATH = re.compile(r"^\s*(~|\.{0,2}/|[A-Za-z]:\\)[\w./\\ -]*\s*$")
WORD = re.compile(r"[A-Za-z']+")
SYMBOL = re.compile(r"[{}()\[\];:=<>+*/&|%!#$@\\]")


@dataclass(frozen=True)
class Plausibility:
    plausible: bool
    score: int
    reasons: tuple[str, ...] = field(default=())


def _prose_heavy(text: str) -> bool:
    """High word-to-symbol ratio: the signature of chat messages and articles."""
    words = len(WORD.findall(text))
    symbols = len(SYMBOL.findall(text))
    if words < 12:
        return False
    return symbols == 0 or words / max(symbols, 1) > 12


def assess(text: str | None) -> Plausibility:
    """Score clipboard content as "looks like a LeetCode solution" or not."""
    if not text or not text.strip():
        return Plausibility(False, 0, ("clipboard is empty",))

    points = 0
    reasons: list[str] = []
    lines = text.strip().splitlines()

    if detect(text).confident:
        points += 3
        reasons.append("language detected")
    if ENTRY_SHAPES.search(text):
        points += 3
        reasons.append("has a solution entry point")
    if len(lines) >= 2:
        points += 1
        reasons.append("multi-line")
    if SYMBOL.search(text) and any(
        line[:1] in (" ", "\t") for line in lines if line.strip()
    ):
        points += 1
        reasons.append("indented code-like punctuation")

    single_line = text.strip()
    if len(lines) == 1 and (
        BARE_URL.match(single_line)
        or BARE_EMAIL.match(single_line)
        or BARE_PATH.match(single_line)
    ):
        points -= 5
        reasons.append("looks like a bare URL, email, or path")
    if len(lines) == 1 and len(single_line) < 40:
        points -= 3
        reasons.append("single short line")
    if _prose_heavy(text):
        points -= 3
        reasons.append("reads as prose")
    if byte_size(text) > LARGE_CLIPBOARD_BYTES:
        points -= 3
        reasons.append("over 200KB")

    return Plausibility(points >= PLAUSIBLE_THRESHOLD, points, tuple(reasons))
