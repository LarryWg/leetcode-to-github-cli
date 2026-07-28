"""Type-to-filter question picker (spec §5.2).

Built directly on prompt_toolkit so filtering happens on every keystroke with
zero network calls: the whole problem set is already in memory.
"""

from __future__ import annotations

import shutil

from prompt_toolkit.application import Application
from prompt_toolkit.buffer import Buffer
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.layout import HSplit, Layout, VSplit, Window
from prompt_toolkit.layout.controls import BufferControl, FormattedTextControl
from prompt_toolkit.layout.dimension import Dimension
from prompt_toolkit.styles import Style

from .errors import Cancelled
from .problems import Question
from .search import ProblemIndex, search

RESULT_LIMIT = 10
MIN_WIDTH = 48

PICKER_STYLE = Style.from_dict(
    {
        "prompt": "bold",
        "row": "",
        "row.selected": "reverse",
        "difficulty.easy": "fg:ansigreen",
        "difficulty.medium": "fg:ansiyellow",
        "difficulty.hard": "fg:ansired",
        "hint": "fg:ansibrightblack",
    }
)


def _difficulty_style(difficulty: str) -> str:
    return f"class:difficulty.{difficulty.lower()}" if difficulty else "class:row"


def format_row(question: Question, width: int) -> str:
    """`1. Two Sum ......... [Easy] 🔒`, difficulty right-aligned."""
    tag = f"[{question.difficulty}]"
    lock = " 🔒" if question.paid else ""
    left = question.display
    room = max(width, MIN_WIDTH) - len(tag) - len(lock) - 4
    if len(left) > room:
        left = left[: max(room - 1, 1)] + "…"
    padding = max(1, room - len(left))
    return f"{left}{' ' * padding}{tag}{lock}"


def pick(index: ProblemIndex, *, initial: str = "") -> Question:
    """Run the interactive picker and return the chosen question.

    Raises Cancelled on Ctrl-C or Esc.
    """
    if not index.questions:
        raise Cancelled("No questions to choose from.")

    state = {"matches": search(index, initial, limit=RESULT_LIMIT), "cursor": 0}

    def refresh(_buffer=None) -> None:
        state["matches"] = search(index, buffer.text, limit=RESULT_LIMIT)
        state["cursor"] = 0

    buffer = Buffer(multiline=False, on_text_changed=refresh)
    buffer.text = initial
    buffer.cursor_position = len(initial)

    def rows():
        width = shutil.get_terminal_size((80, 24)).columns - 6
        matches = state["matches"]
        if not matches:
            return [("class:hint", "    no matches")]
        fragments: list[tuple[str, str]] = []
        for position, question in enumerate(matches):
            selected = position == state["cursor"]
            marker = "  ❯ " if selected else "    "
            style = "class:row.selected" if selected else "class:row"
            tag = f"[{question.difficulty}]"
            lock = " 🔒" if question.paid else ""
            room = max(width, MIN_WIDTH) - len(tag) - len(lock) - 4
            left = question.display
            if len(left) > room:
                left = left[: max(room - 1, 1)] + "…"
            fragments.append((style, f"{marker}{left}{' ' * max(1, room - len(left))}"))
            fragments.append((_difficulty_style(question.difficulty), f"{tag}{lock}"))
            fragments.append(("", "\n"))
        fragments.append(("class:hint", "    ↑/↓ to move, Enter to select"))
        return fragments

    keys = KeyBindings()

    @keys.add("up")
    @keys.add("c-p")
    def _up(_event) -> None:
        if state["matches"]:
            state["cursor"] = (state["cursor"] - 1) % len(state["matches"])

    @keys.add("down")
    @keys.add("c-n")
    def _down(_event) -> None:
        if state["matches"]:
            state["cursor"] = (state["cursor"] + 1) % len(state["matches"])

    @keys.add("enter")
    def _accept(event) -> None:
        matches = state["matches"]
        if matches:
            event.app.exit(result=matches[state["cursor"]])

    @keys.add("c-c")
    @keys.add("escape", eager=True)
    def _cancel(event) -> None:
        event.app.exit(result=None)

    layout = Layout(
        HSplit(
            [
                VSplit(
                    [
                        Window(
                            FormattedTextControl([("class:prompt", "? Question:  ")]),
                            width=13,
                            height=1,
                        ),
                        Window(BufferControl(buffer=buffer), height=1),
                    ]
                ),
                Window(
                    FormattedTextControl(rows),
                    height=Dimension(min=1, max=RESULT_LIMIT + 1),
                ),
            ]
        )
    )

    app: Application = Application(
        layout=layout, key_bindings=keys, style=PICKER_STYLE, full_screen=False
    )
    result = app.run()
    if result is None:
        raise Cancelled()
    return result
