"""Target paths and commit-message rendering (spec §8.1-8.2)."""

from __future__ import annotations

import re
import string
from dataclasses import dataclass

from .config import DEFAULT_MESSAGE_TEMPLATE, DEFAULT_UPDATE_TEMPLATE
from .detect import Language
from .problems import Question

SUBJECT_SOFT_LIMIT = 72


def target_path(prefix: str, question: Question, language: Language) -> str:
    """`{path}{id:0>4}-{slug}{ext}` — e.g. `solutions/0001-two-sum.py`."""
    return f"{prefix}{question.padded_id}-{question.slug}{language.ext}"


def filename(question: Question, language: Language) -> str:
    return f"{question.padded_id}-{question.slug}{language.ext}"


def message_vars(
    question: Question, language: Language, *, lines: int, prefix: str = ""
) -> dict[str, str]:
    name = filename(question, language)
    return {
        "id": question.id,
        "padded_id": question.padded_id,
        "title": question.title,
        "slug": question.slug,
        "language": language.label,
        "ext": language.ext,
        "difficulty": question.difficulty,
        "filename": name,
        "path": f"{prefix}{name}",
        "lines": str(lines),
    }


@dataclass(frozen=True)
class RenderedMessage:
    text: str
    warning: str | None = None


def render_template(
    template: str, variables: dict[str, str], *, fallback: str
) -> RenderedMessage:
    """Render a template, degrading to `fallback` rather than ever crashing.

    A misspelled variable in a user's config must not take the tool down
    (spec §8.2) — it warns once and uses the built-in default.
    """
    try:
        text = string.Formatter().vformat(template, (), variables)
    except (KeyError, IndexError, ValueError, AttributeError) as exc:
        detail = str(exc).strip("'\"") or exc.__class__.__name__
        warning = f"⚠ Commit template is invalid ({detail}); using the built-in default."
        return RenderedMessage(fallback.format(**variables).strip(), warning)
    return RenderedMessage(text.strip())


def render_message(
    question: Question,
    language: Language,
    *,
    lines: int,
    updating: bool,
    message_template: str,
    update_template: str,
    prefix: str = "",
) -> RenderedMessage:
    """The template that applies, rendered — never blank, never a crash."""
    variables = message_vars(question, language, lines=lines, prefix=prefix)
    template = update_template if updating else message_template
    fallback = DEFAULT_UPDATE_TEMPLATE if updating else DEFAULT_MESSAGE_TEMPLATE
    rendered = render_template(template, variables, fallback=fallback)
    if not rendered.text.strip():
        return RenderedMessage(
            fallback.format(**variables).strip(),
            "⚠ Commit template rendered empty; using the built-in default.",
        )
    return rendered


def clean_message(message: str) -> str:
    """Trim trailing whitespace per line; preserve the body of a multi-line message."""
    lines = [line.rstrip() for line in message.replace("\r\n", "\n").split("\n")]
    return "\n".join(lines).strip()


def subject_warning(message: str) -> str | None:
    subject = message.split("\n", 1)[0]
    if len(subject) > SUBJECT_SOFT_LIMIT:
        return (
            f"⚠ Commit subject is {len(subject)} characters "
            f"(over {SUBJECT_SOFT_LIMIT}); accepted anyway."
        )
    return None


_TEMPLATE_FIELD = re.compile(r"\{(\w+)")


def unknown_template_fields(template: str, variables: dict[str, str]) -> tuple[str, ...]:
    """Variable names in `template` that lcpush cannot fill."""
    return tuple(
        sorted({name for name in _TEMPLATE_FIELD.findall(template) if name not in variables})
    )
