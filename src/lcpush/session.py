"""The `lcpush` run flow: pick a question, read a solution, push it (spec §3)."""

from __future__ import annotations

import sys
from dataclasses import dataclass

import questionary

from . import clipboard, editor, onboarding, prompts, tokens, ui
from .config import Config, load
from .detect import Detection, Language, detect, resolve_language
from .errors import Cancelled, InputError
from .github import GitHubClient
from .picker import pick
from .plausibility import assess
from .problems import Question, get_questions
from .render import (
    clean_message,
    filename as render_filename,
    render_message,
    subject_warning,
    target_path,
)
from .search import build_index
from .solution import line_count, normalize, reject_reason, soft_warnings


@dataclass(frozen=True)
class SolutionInput:
    text: str
    source_label: str
    detection: Detection


# -- configuration ---------------------------------------------------------


def load_config() -> tuple[Config, str]:
    """Return (config, token), running first-run setup when needed."""
    config = load()
    if config is None or not config.repo.configured:
        return onboarding.setup(config)
    token = onboarding.resolve_token(interactive=True)
    return config, token


# -- question selection ----------------------------------------------------


def choose_question(config: Config, *, refresh: bool = False) -> Question:
    """Run the fuzzy picker over the cached problem set."""
    questions = get_questions(
        ttl_days=config.cache.problems_ttl_days,
        refresh=refresh,
        warn=lambda message: ui.dim(f"  {message}"),
        info=lambda message: ui.dim(f"  {message}"),
    )
    question = pick(build_index(questions))
    ui.success(question.display)
    return question


# -- solution input --------------------------------------------------------


def _source_choices() -> tuple[list, str]:
    """Menu entries plus the preselected value, ordered by plausibility (§6.2)."""
    entries: list = []
    clip_text = clipboard.read()
    plausible = assess(clip_text) if clip_text else None
    default = "editor"

    if clip_text is not None:
        if plausible is not None and plausible.plausible:
            entries.append(questionary.Choice(title="Clipboard", value="clipboard"))
            default = "clipboard"
        else:
            entries.append(
                questionary.Choice(
                    title="Clipboard (doesn't look like code)", value="clipboard"
                )
            )
    entries.append(questionary.Choice(title="Editor", value="editor"))
    entries.append(questionary.Choice(title="Stdin", value="stdin"))
    return entries, default


def _read_source(source: str) -> tuple[str, str]:
    """(text, label) for a chosen source. Raises InputError when unusable."""
    if source == "clipboard":
        text = clipboard.read()
        if text is None:
            raise InputError("Clipboard is empty.")
        return text, "Clipboard"
    if source == "editor":
        text = editor.open_editor()
        if text is None:
            raise InputError("No solution provided. Aborted.")
        return text, "Editor"
    ui.dim("  Paste your solution, then Ctrl-D (or a lone EOF line) to finish.")
    return editor.read_stdin(), "Stdin"


def read_solution(question: Question) -> SolutionInput:
    """Read, validate, preview and confirm a solution (spec §6).

    Declining the preview returns to the source menu rather than exiting, so a
    stale clipboard costs one keypress.
    """
    while True:
        entries, default = _source_choices()
        source = prompts.select("? Solution source:", entries, default=default)
        try:
            raw, label = _read_source(source)
        except InputError as exc:
            # A clipboard that emptied since the menu was drawn is worth a
            # retry; an empty editor buffer is an explicit abort (§9).
            if source != "clipboard":
                raise
            ui.warn(f"⚠ {exc.message}")
            continue

        problem = reject_reason(raw)
        if problem:
            ui.warn(f"⚠ {problem}")
            continue

        text = normalize(raw)
        detection = detect(text)
        ui.success(f"Read {line_count(text)} lines from {label.lower()}")

        warnings = soft_warnings(
            text, detection=detection, slug=question.slug, title=question.title
        )
        ui.info("")
        ui.info(ui.render_preview(label, text, detection.label))
        for message in warnings:
            ui.warn(message)
        if prompts.confirm("  └ Use this?", default=True):
            return SolutionInput(text, label, detection)
        ui.info("")


# -- language --------------------------------------------------------------


def choose_language(solution: SolutionInput) -> Language:
    detection = solution.detection
    entries = [
        questionary.Choice(
            title=f"{language.label}  (detected)"
            if language is detection.language
            else language.label,
            value=language.key,
        )
        for language, _ in detection.ranked
    ]
    default = detection.language.key if detection.language else None
    if default is None:
        ui.warn("⚠ Could not identify a programming language — choose one.")
    chosen = prompts.select("? Language:", entries, default=default)
    return resolve_language(chosen) or detection.ranked[0][0]


# -- commit message and push ----------------------------------------------


def build_message(
    question: Question,
    language: Language,
    config: Config,
    *,
    lines: int,
    updating: bool,
) -> str:
    rendered = render_message(
        question,
        language,
        lines=lines,
        updating=updating,
        message_template=config.commit.message_template,
        update_template=config.commit.update_template,
        prefix=config.repo.path,
    )
    if rendered.warning:
        ui.warn(rendered.warning)
    message = rendered.text

    if config.commit.prompt == "always":
        message = edit_message(message)
    return message


def edit_message(current: str, *, in_editor: bool = False) -> str:
    """Re-prompt until non-empty; never silently fall through to a default."""
    while True:
        if in_editor:
            edited = editor.open_editor(
                initial=current,
                header="# Edit the commit message. Lines starting with # are kept.\n",
                suffix=".txt",
            )
            candidate = clean_message(edited or "")
        else:
            ui.dim("    Enter to confirm, Ctrl-U to clear")
            candidate = clean_message(
                prompts.edit_line("? Commit message:  ", default=current)
            )
        if candidate:
            warning = subject_warning(candidate)
            if warning:
                ui.warn(warning)
            return candidate
        ui.warn("⚠ Commit message cannot be empty.")


def confirm_push(
    *,
    path: str,
    lines: int,
    repo: str,
    branch: str,
    message: str,
    updating: bool,
    prompt_mode: str,
) -> str:
    """Draw the ready-to-push panel until the user pushes or cancels.

    Returns the final commit message; raises Cancelled on `n`.
    """
    current = message
    editable = prompt_mode != "never"
    while True:
        ui.info("")
        ui.info(
            ui.render_push_panel(
                filename=path,
                lines=lines,
                repo=repo,
                branch=branch,
                message=current,
                updating=updating,
                prompt_mode=prompt_mode,
            )
        )
        allowed = {"enter": "push", "n": "cancel", "escape": "cancel"}
        if editable:
            allowed |= {"m": "edit", "M": "editor"}
        action = prompts.read_key(allowed)
        if action == "push":
            return current
        if action == "cancel":
            raise Cancelled("Cancelled — nothing was pushed.")
        current = edit_message(current, in_editor=action == "editor")


def push(
    config: Config,
    token: str,
    question: Question,
    language: Language,
    solution: SolutionInput,
) -> str:
    """Resolve add-vs-overwrite, confirm, and PUT the file. Returns the URL."""
    path = target_path(config.repo.path, question, language)
    name = render_filename(question, language)
    lines = line_count(solution.text)

    with GitHubClient(token) as github:
        sha = github.get_file_sha(
            config.repo.owner, config.repo.name, path, config.repo.branch
        )
        updating = sha is not None

        message = build_message(
            question, language, config, lines=lines, updating=updating
        )
        message = confirm_push(
            path=name,
            lines=lines,
            repo=config.repo.full_name,
            branch=config.repo.branch,
            message=message,
            updating=updating,
            prompt_mode=config.commit.prompt,
        )

        ui.info("")
        ui.arrow(
            f"Pushing {path} to {config.repo.full_name} ({config.repo.branch})"
        )
        result = github.put_file(
            config.repo.owner,
            config.repo.name,
            path,
            content=solution.text,
            message=message,
            branch=config.repo.branch,
            sha=sha,
            author_name=config.commit.author_name,
            author_email=config.commit.author_email,
        )
    return result.html_url


def run(*, refresh: bool = False) -> int:
    """Entry point for a full push. Returns a process exit code."""
    if not sys.stdin.isatty():
        raise InputError("lcpush is interactive and needs a terminal.")

    config, token = load_config()
    question = choose_question(config, refresh=refresh)
    solution = read_solution(question)
    language = choose_language(solution)
    url = push(config, token, question, language, solution)
    ui.success(tokens.redact(url, token))
    return 0
