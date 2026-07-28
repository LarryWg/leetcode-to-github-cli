"""Fuzzy problem search — pure, offline, no network calls (spec §5.2).

The index is built once per session; `search` runs on every keystroke, so it
stays allocation-light and leans on rapidfuzz's C++ scorers.
"""

from __future__ import annotations

from dataclasses import dataclass

from rapidfuzz import fuzz, process

from .problems import Question

SCORE_CUTOFF = 55.0


@dataclass(frozen=True)
class ProblemIndex:
    questions: tuple[Question, ...]
    displays: tuple[str, ...]
    slugs: tuple[str, ...]


def build_index(questions: tuple[Question, ...]) -> ProblemIndex:
    return ProblemIndex(
        questions=tuple(questions),
        displays=tuple(q.display for q in questions),
        slugs=tuple(q.slug for q in questions),
    )


def _sort_key(question: Question) -> tuple[int, int, str]:
    if question.id.isdigit():
        return (0, int(question.id), question.id)
    return (1, 0, question.id)


def search(index: ProblemIndex, query: str, *, limit: int = 10) -> tuple[Question, ...]:
    """Top `limit` matches for `query`, best first.

    An all-digit query exact-prefix-matches on the problem id first; anything
    else is fuzzy-matched against both "{id}. {title}" and the slug, taking the
    better of the two scores.
    """
    text = query.strip()
    if not text:
        return tuple(sorted(index.questions, key=_sort_key)[:limit])

    results: list[Question] = []
    taken: set[int] = set()

    if text.isdigit():
        prefixed = sorted(
            (
                (position, question)
                for position, question in enumerate(index.questions)
                if question.id.startswith(text)
            ),
            key=lambda pair: _sort_key(pair[1]),
        )
        for position, question in prefixed[:limit]:
            results.append(question)
            taken.add(position)
        if len(results) >= limit:
            return tuple(results)

    scores: dict[int, float] = {}
    for choices in (index.displays, index.slugs):
        for _, score, position in process.extract(
            text,
            choices,
            scorer=fuzz.WRatio,
            limit=limit * 5,
            score_cutoff=SCORE_CUTOFF,
        ):
            if position in taken:
                continue
            if score > scores.get(position, 0.0):
                scores[position] = score

    ranked = sorted(
        scores.items(),
        key=lambda pair: (-pair[1], _sort_key(index.questions[pair[0]])),
    )
    for position, _ in ranked:
        results.append(index.questions[position])
        if len(results) >= limit:
            break
    return tuple(results)
