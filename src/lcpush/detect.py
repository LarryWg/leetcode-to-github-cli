"""Language detection by weighted regex signals (spec §7).

Detection is a ranking, not a verdict: the caller always shows the full list
sorted by score with the winner preselected, and confirmation is the safety
net. When nothing clears `THRESHOLD`, nothing is preselected.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

THRESHOLD = 4


@dataclass(frozen=True)
class Language:
    key: str
    label: str
    ext: str


LANGUAGES: tuple[Language, ...] = (
    Language("cpp", "C++", ".cpp"),
    Language("c", "C", ".c"),
    Language("python3", "Python3", ".py"),
    Language("java", "Java", ".java"),
    Language("csharp", "C#", ".cs"),
    Language("javascript", "JavaScript", ".js"),
    Language("typescript", "TypeScript", ".ts"),
    Language("golang", "Go", ".go"),
    Language("rust", "Rust", ".rs"),
    Language("ruby", "Ruby", ".rb"),
    Language("swift", "Swift", ".swift"),
    Language("kotlin", "Kotlin", ".kt"),
    Language("scala", "Scala", ".scala"),
    Language("php", "PHP", ".php"),
)

BY_KEY = {lang.key: lang for lang in LANGUAGES}

# Aliases accepted by --lang, so `--lang python`/`py`/`c++` all work.
ALIASES = {
    "c++": "cpp",
    "cc": "cpp",
    "cxx": "cpp",
    "py": "python3",
    "python": "python3",
    "py3": "python3",
    "c#": "csharp",
    "cs": "csharp",
    "js": "javascript",
    "node": "javascript",
    "ts": "typescript",
    "go": "golang",
    "rs": "rust",
    "rb": "ruby",
    "kt": "kotlin",
}


def resolve_language(name: str) -> Language | None:
    key = name.strip().lower()
    key = ALIASES.get(key, key)
    return BY_KEY.get(key)


def _rx(pattern: str) -> re.Pattern[str]:
    return re.compile(pattern, re.MULTILINE)


# (pattern, weight) per language.
SIGNALS: dict[str, tuple[tuple[re.Pattern[str], int], ...]] = {
    "cpp": (
        (_rx(r"#include\s*<"), 2),
        (_rx(r"std::"), 3),
        (_rx(r"\bvector<"), 3),
        (_rx(r"^\s*public:"), 3),
        (_rx(r"\bnullptr\b"), 2),
        (_rx(r"\bclass\s+Solution\s*\{"), 1),
    ),
    "c": (
        (_rx(r"#include\s*<(stdlib|stdio|string)\.h>"), 3),
        (_rx(r"\bmalloc\s*\("), 3),
        (_rx(r"\bint\s*\*"), 2),
        (_rx(r"\breturnSize\b"), 3),
        (_rx(r"\bcalloc\s*\(|\bfree\s*\("), 2),
    ),
    "python3": (
        (_rx(r"^\s*class\s+Solution\s*(\(object\))?\s*:"), 4),
        (_rx(r"^\s*def\s+\w+\s*\(\s*self"), 4),
        (_rx(r"\b(List|Dict|Optional|Tuple)\[" ), 2),
        (_rx(r"^\s*def\s+\w+\s*\(.*\)\s*->"), 2),
        (_rx(r"^\s*(from|import)\s+\w+"), 1),
    ),
    "java": (
        (_rx(r"^\s*import\s+java\."), 4),
        (_rx(r"\bpublic\s+class\s+Solution\b"), 2),
        (_rx(r"\bint\[\]\s+\w+"), 2),
        (_rx(r"\bnew\s+(ArrayList|HashMap|HashSet)<"), 3),
        (_rx(r"\bSystem\.out\."), 2),
    ),
    "csharp": (
        (_rx(r"^\s*using\s+System"), 4),
        (_rx(r"\bIList<|\bIDictionary<"), 4),
        (_rx(r"\bpublic\s+class\s+Solution\b"), 1),
        (_rx(r"\bpublic\s+[\w\[\]<>,\s]+\s+[A-Z]\w*\s*\("), 2),
        (_rx(r"\bvar\s+\w+\s*=\s*new\s+\w"), 1),
    ),
    "javascript": (
        (_rx(r"@param\s*\{"), 3),
        (_rx(r"\b(var|const|let)\s+\w+\s*=\s*function\s*\("), 4),
        (_rx(r"\bmodule\.exports\b"), 2),
        (_rx(r"\bfunction\s*\w*\s*\([^)]*\)\s*\{"), 1),
        (_rx(r"===|!=="), 1),
    ),
    "typescript": (
        (_rx(r"function\s+\w+\s*\([^)]*\w+\s*:\s*\w"), 4),
        (_rx(r"\)\s*:\s*(number|string|boolean|void|any)(\[\])?\s*(\{|=>)"), 4),
        (_rx(r":\s*(number|string|boolean)\[\]"), 3),
        (_rx(r"^\s*(interface|type)\s+\w+"), 2),
    ),
    "golang": (
        (_rx(r"^\s*package\s+\w+"), 4),
        (_rx(r":="), 3),
        (_rx(r"^\s*func\s+\w+\s*\("), 2),
        (_rx(r"\[\]int\b|\[\]string\b"), 3),
        (_rx(r"^\s*import\s*\("), 1),
    ),
    "rust": (
        (_rx(r"\bimpl\s+Solution\b"), 4),
        (_rx(r"\bpub\s+fn\b"), 4),
        (_rx(r"\bVec<"), 3),
        (_rx(r"&self\b|&mut\b"), 2),
        (_rx(r"\bi32\b|\bi64\b|\busize\b"), 2),
    ),
    "ruby": (
        (_rx(r"@param\s*\{\w+"), 2),
        (_rx(r"^\s*def\s+\w+.*$"), 2),
        (_rx(r"^\s*end\s*$"), 3),
        (_rx(r"\bnil\b"), 2),
        (_rx(r"\.each\s+do\s*\||\.each_with_index"), 3),
        (_rx(r"^\s*#\s*@(param|return)"), 2),
    ),
    "swift": (
        (_rx(r"\bfunc\s+\w+\s*\(.*\)\s*->\s*\["), 4),
        (_rx(r"\bclass\s+Solution\s*\{"), 1),
        (_rx(r"^\s*(var|let)\s+\w+\s*(=|:)"), 2),
        (_rx(r"\[Int\]|\[String\]"), 3),
        (_rx(r"\bguard\s+let\b|\bif\s+let\b"), 2),
    ),
    "kotlin": (
        (_rx(r"\bfun\s+\w+\s*\("), 4),
        (_rx(r"\bIntArray\b|\bMutableList<"), 4),
        (_rx(r"^\s*val\s+\w+"), 2),
        (_rx(r"\bclass\s+Solution\s*\{"), 1),
    ),
    "scala": (
        (_rx(r"\bobject\s+Solution\b"), 4),
        (_rx(r"\bdef\s+\w+\s*\(.*\)\s*:\s*Array\["), 4),
        (_rx(r":\s*Array\[|\bList\[Int\]"), 3),
        (_rx(r"\bval\s+\w+\s*="), 1),
    ),
    "php": (
        (_rx(r"<\?php"), 4),
        (_rx(r"\bfunction\s+\w+\s*\([^)]*\$"), 4),
        (_rx(r"\$\w+\s*="), 2),
        (_rx(r"->\w+\(|\barray\s*\("), 1),
    ),
}

# Ambiguity gates (spec §7). Each returns a score delta keyed by language.
_HAS_CLASS = _rx(r"\bclass\b")
_HAS_STD = _rx(r"std::|#include\s*<(iostream|vector|string|unordered_map)>")
_CSHARP_FORCE = _rx(r"^\s*using\s+System|\bIList<")
_TS_ANNOTATION = _rx(
    r"\(\s*\w+\s*:\s*\w|,\s*\w+\s*:\s*\w+[\[\]]*\s*[,)]|\)\s*:\s*\w+(\[\])?\s*(\{|=>)"
)


def _apply_gates(text: str, scores: dict[str, int]) -> dict[str, int]:
    """Resolve C/C++, Java/C#, and JS/TS with hard rules rather than weights."""
    adjusted = dict(scores)
    if _HAS_CLASS.search(text) or _HAS_STD.search(text):
        adjusted["c"] = 0
    if _CSHARP_FORCE.search(text):
        adjusted["java"] = max(0, adjusted.get("java", 0) - 4)
    if _TS_ANNOTATION.search(text) and adjusted.get("typescript", 0) > 0:
        adjusted["javascript"] = max(0, adjusted.get("javascript", 0) - 3)
    else:
        adjusted["typescript"] = max(0, adjusted.get("typescript", 0) - 2)
    return adjusted


def score(text: str) -> dict[str, int]:
    """Raw per-language scores after ambiguity gates."""
    raw = {
        key: sum(weight for pattern, weight in signals if pattern.search(text))
        for key, signals in SIGNALS.items()
    }
    return _apply_gates(text, raw)


def rank(text: str) -> tuple[tuple[Language, int], ...]:
    """Every supported language, highest score first, stable within ties."""
    scores = score(text)
    order = {lang.key: position for position, lang in enumerate(LANGUAGES)}
    return tuple(
        sorted(
            ((lang, scores.get(lang.key, 0)) for lang in LANGUAGES),
            key=lambda pair: (-pair[1], order[pair[0].key]),
        )
    )


@dataclass(frozen=True)
class Detection:
    language: Language | None
    score: int
    ranked: tuple[tuple[Language, int], ...]

    @property
    def confident(self) -> bool:
        return self.language is not None

    @property
    def label(self) -> str:
        return self.language.label if self.language else "unknown"


def detect(text: str) -> Detection:
    """Best-scoring language, or a Detection with `language=None` below threshold."""
    ranked = rank(text)
    if not text.strip():
        return Detection(None, 0, ranked)
    best, best_score = ranked[0]
    if best_score < THRESHOLD:
        return Detection(None, best_score, ranked)
    return Detection(best, best_score, ranked)
