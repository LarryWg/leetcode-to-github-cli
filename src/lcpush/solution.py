"""Solution content: normalization, hard rejections, soft warnings (spec §6.4-6.6)."""

from __future__ import annotations

import re
from dataclasses import dataclass

from rapidfuzz import fuzz

from .detect import Detection

MAX_BYTES = 1024 * 1024  # 1MB hard rejection
LARGE_CLIPBOARD_BYTES = 200 * 1024  # 200KB plausibility penalty

ENTRY_POINT_PATTERNS = (
    re.compile(r"^\s*def\s+(\w+)\s*\(", re.MULTILINE),  # Python / Ruby
    re.compile(r"^\s*(?:pub\s+)?fn\s+(\w+)\s*[(<]", re.MULTILINE),  # Rust / Swift-ish
    re.compile(r"^\s*func\s+(\w+)\s*[(<]", re.MULTILINE),  # Go / Swift
    re.compile(r"^\s*fun\s+(\w+)\s*\(", re.MULTILINE),  # Kotlin
    re.compile(
        r"^\s*(?:public|private|protected|static|final|\s)*[\w<>\[\],\s*&:]+?\s+(\w+)\s*\([^;]*\)\s*\{",
        re.MULTILINE,
    ),  # C-family / Java / C#
    re.compile(r"\b(?:var|const|let)\s+(\w+)\s*=\s*function\s*\(", re.MULTILINE),
    re.compile(r"^\s*function\s+(\w+)\s*\(", re.MULTILINE),
)

_SKIP_ENTRY_NAMES = frozenset(
    {
        "if",
        "for",
        "while",
        "switch",
        "catch",
        "return",
        "main",
        "new",
        "class",
        "struct",
        "__init__",
        "Solution",
    }
)

CONFLICT_MARKERS = ("<<<<<<<", ">>>>>>>", "=======")
OPENERS = {"(": ")", "[": "]", "{": "}"}
CLOSERS = {value: key for key, value in OPENERS.items()}


def normalize(text: str) -> str:
    """CRLF -> LF, strip trailing whitespace per line, exactly one trailing newline.

    Nothing else is touched: no reformatting, no linting (spec §6.6).
    """
    unified = text.replace("\r\n", "\n").replace("\r", "\n")
    lines = [line.rstrip() for line in unified.split("\n")]
    body = "\n".join(lines).rstrip("\n")
    return f"{body}\n" if body else ""


def line_count(text: str) -> int:
    stripped = text.rstrip("\n")
    return len(stripped.split("\n")) if stripped else 0


def byte_size(text: str) -> int:
    return len(text.encode("utf-8"))


def reject_reason(text: str) -> str | None:
    """The only two hard rejections (spec §6.5); both re-prompt, never exit."""
    if not text or not text.strip():
        return "Content is empty."
    if byte_size(text) > MAX_BYTES:
        return f"Content is {byte_size(text) // 1024}KB, over the 1MB limit."
    return None


def entry_point_names(text: str) -> tuple[str, ...]:
    """Plausible entry-point function names, in source order, deduplicated."""
    found: list[str] = []
    for pattern in ENTRY_POINT_PATTERNS:
        for match in pattern.finditer(text):
            name = match.group(1)
            if name in _SKIP_ENTRY_NAMES or name in found:
                continue
            found.append(name)
    return tuple(found)


def brackets_balanced(text: str) -> bool:
    """Rough balance check that skips string literals and line comments."""
    stack: list[str] = []
    index = 0
    length = len(text)
    while index < length:
        char = text[index]
        if char in "\"'":
            quote = char
            index += 1
            while index < length:
                if text[index] == "\\":
                    index += 2
                    continue
                if text[index] == quote or text[index] == "\n":
                    break
                index += 1
            index += 1
            continue
        if char == "#" or (char == "/" and text[index : index + 2] == "//"):
            newline = text.find("\n", index)
            index = length if newline == -1 else newline
            continue
        if text[index : index + 2] == "/*":
            end = text.find("*/", index + 2)
            index = length if end == -1 else end + 2
            continue
        if char in OPENERS:
            stack.append(char)
        elif char in CLOSERS:
            if not stack or stack[-1] != CLOSERS[char]:
                return False
            stack.pop()
        index += 1
    return not stack


def _slug_variants(slug: str) -> tuple[str, ...]:
    parts = [part for part in slug.split("-") if part]
    if not parts:
        return ()
    camel = parts[0] + "".join(part.capitalize() for part in parts[1:])
    return (camel, "_".join(parts), "".join(parts))


def matches_slug(name: str, slug: str) -> bool:
    """Does an entry-point name plausibly correspond to the question slug?

    Deliberately generous: LeetCode's naming is inconsistent and this only ever
    drives a warning (spec §6.4).
    """
    if not slug:
        return True
    flat_name = re.sub(r"[^a-z0-9]", "", name.lower())
    if not flat_name:
        return True
    for variant in _slug_variants(slug):
        flat_variant = re.sub(r"[^a-z0-9]", "", variant.lower())
        if not flat_variant:
            continue
        if flat_name == flat_variant:
            return True
        if flat_name in flat_variant or flat_variant in flat_name:
            return True
        if fuzz.ratio(flat_name, flat_variant) >= 70:
            return True
    return False


@dataclass(frozen=True)
class Warning_:
    code: str
    message: str


def soft_warnings(
    text: str,
    *,
    detection: Detection | None = None,
    slug: str = "",
    title: str = "",
) -> tuple[str, ...]:
    """Non-blocking warnings printed above the confirm prompt (spec §6.4)."""
    messages: list[str] = []

    if detection is not None and not detection.confident:
        messages.append("⚠ Could not identify a programming language.")

    if slug:
        names = entry_point_names(text)
        if names and not any(matches_slug(name, slug) for name in names):
            label = title or slug
            messages.append(
                f'⚠ This defines {names[0]}, but you selected "{label}". Wrong question?'
            )

    if any(marker in text for marker in ("<<<<<<<", ">>>>>>>")) or "TODO" in text:
        messages.append("⚠ Content contains conflict markers or TODOs.")

    if not brackets_balanced(text):
        messages.append("⚠ Brackets are unbalanced — the paste may be truncated.")

    return tuple(messages)
