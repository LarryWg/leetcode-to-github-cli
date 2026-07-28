"""Interactive prompt helpers.

Thin wrappers over questionary/prompt_toolkit so the session flow reads as a
sequence of intentions, and so Ctrl-C turns into `Cancelled` exactly once.
"""

from __future__ import annotations

import questionary
from prompt_toolkit.application import Application
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.layout import Layout, Window
from prompt_toolkit.layout.controls import FormattedTextControl
from prompt_toolkit.shortcuts import PromptSession

from .errors import Cancelled

QUESTIONARY_STYLE = questionary.Style(
    [
        ("qmark", "fg:ansicyan bold"),
        ("question", "bold"),
        ("answer", "fg:ansigreen bold"),
        ("pointer", "fg:ansicyan bold"),
        ("highlighted", "fg:ansicyan bold"),
        ("selected", "fg:ansigreen"),
    ]
)


def _unwrap(value):
    if value is None:
        raise Cancelled()
    return value


def select(message: str, choices: list, *, default=None):
    """Single-select menu; `choices` may hold questionary.Choice objects."""
    return _unwrap(
        questionary.select(
            message,
            choices=choices,
            default=default,
            style=QUESTIONARY_STYLE,
            instruction="↑/↓ to change, Enter to accept",
        ).ask()
    )


def confirm(message: str, *, default: bool = True) -> bool:
    return _unwrap(
        questionary.confirm(message, default=default, style=QUESTIONARY_STYLE).ask()
    )


def text(message: str, *, default: str = "") -> str:
    """Free text with `default` pre-filled and the cursor at the end."""
    return _unwrap(
        questionary.text(message, default=default, style=QUESTIONARY_STYLE).ask()
    )


def password(message: str) -> str:
    return _unwrap(questionary.password(message, style=QUESTIONARY_STYLE).ask())


def edit_line(message: str, *, default: str) -> str:
    """Pre-filled single-line editor: accepting is one keypress (spec §8.2).

    Ctrl-U clears the line (prompt_toolkit's emacs binding), matching the hint
    shown in the spec's mock.
    """
    session: PromptSession = PromptSession()
    try:
        return session.prompt(message, default=default)
    except (KeyboardInterrupt, EOFError) as exc:
        raise Cancelled() from exc


def read_key(allowed: dict[str, str]) -> str:
    """Block for a single keypress. `allowed` maps prompt_toolkit keys to results."""
    keys = KeyBindings()

    def bind(key: str, result: str):
        @keys.add(key, eager=True)
        def _handler(event, _result=result) -> None:
            event.app.exit(result=_result)

    for key, result in allowed.items():
        bind(key, result)

    @keys.add("c-c")
    @keys.add("c-d")
    def _cancel(event) -> None:
        event.app.exit(result="__cancel__")

    app: Application = Application(
        layout=Layout(Window(FormattedTextControl("", focusable=True), height=0)),
        key_bindings=keys,
        full_screen=False,
    )
    result = app.run()
    if result == "__cancel__":
        raise Cancelled()
    return result
