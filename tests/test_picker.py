"""The prompt_toolkit UIs, driven through a pipe input (no real terminal)."""

from __future__ import annotations

import pytest
from prompt_toolkit.application import create_app_session
from prompt_toolkit.input import create_pipe_input
from prompt_toolkit.output import DummyOutput

from lcpush.errors import Cancelled
from lcpush.picker import format_row, pick
from lcpush.prompts import confirm, edit_line, read_key, select
from lcpush.search import build_index


def _drive(keys: str, run):
    with create_pipe_input() as pipe:
        pipe.send_text(keys)
        with create_app_session(input=pipe, output=DummyOutput()):
            return run()


def test_typing_filters_and_enter_selects(questions):
    index = build_index(questions)
    chosen = _drive("two su\r", lambda: pick(index))
    assert chosen.slug == "two-sum"


def test_arrow_keys_move_the_cursor(questions):
    index = build_index(questions)
    chosen = _drive("two su\x1b[B\r", lambda: pick(index))  # down, enter
    assert chosen.slug != "two-sum"
    assert "two-sum" in chosen.slug  # still within the filtered set


def test_digits_jump_to_an_id(questions):
    index = build_index(questions)
    assert _drive("167\r", lambda: pick(index)).id == "167"


def test_escape_cancels(questions):
    index = build_index(questions)
    with pytest.raises(Cancelled):
        _drive("\x1b", lambda: pick(index))


def test_ctrl_c_cancels(questions):
    index = build_index(questions)
    with pytest.raises(Cancelled):
        _drive("\x03", lambda: pick(index))


def test_empty_index_is_cancelled():
    with pytest.raises(Cancelled):
        pick(build_index(()))


def test_format_row_right_aligns_difficulty_and_marks_paid(questions):
    row = format_row(questions[0], 60)
    assert row.startswith("1. Two Sum")
    assert row.rstrip().endswith("[Easy]")
    assert format_row(questions[-1], 60).rstrip().endswith("[Easy] 🔒")


def test_format_row_truncates_long_titles(questions):
    row = format_row(questions[2], 48)
    assert "…" in row
    assert row.rstrip().endswith("[Medium]")


@pytest.mark.parametrize(
    "key,expected", [("\r", "push"), ("m", "edit"), ("M", "editor"), ("n", "cancel")]
)
def test_read_key_maps_each_panel_key(key, expected):
    allowed = {"enter": "push", "m": "edit", "M": "editor", "n": "cancel"}
    assert _drive(key, lambda: read_key(allowed)) == expected


def test_read_key_ctrl_c_cancels():
    with pytest.raises(Cancelled):
        _drive("\x03", lambda: read_key({"enter": "push"}))


def _source_choices():
    import questionary

    return [
        questionary.Choice(title="Clipboard (doesn't look like code)", value="clipboard"),
        questionary.Choice(title="Editor", value="editor"),
        questionary.Choice(title="Stdin", value="stdin"),
    ]


def test_select_honours_a_value_default():
    """questionary must accept the plain value we pass as `default`."""
    chosen = _drive(
        "\r", lambda: select("? Solution source:", _source_choices(), default="editor")
    )
    assert chosen == "editor"


def test_select_arrow_then_enter():
    chosen = _drive(
        "\x1b[B\r",
        lambda: select("? Solution source:", _source_choices(), default="editor"),
    )
    assert chosen == "stdin"


def test_select_ctrl_c_becomes_cancelled():
    with pytest.raises(Cancelled):
        _drive("\x03", lambda: select("? Solution source:", _source_choices()))


def test_confirm_defaults_to_yes_on_enter():
    assert _drive("\r", lambda: confirm("  └ Use this?", default=True)) is True
    assert _drive("n\r", lambda: confirm("  └ Use this?", default=True)) is False


def test_edit_line_starts_pre_filled():
    """Acceptance 13: Enter alone accepts the pre-filled message."""
    assert (
        _drive("\r", lambda: edit_line("? Commit message:  ", default="Add 1. Two Sum"))
        == "Add 1. Two Sum"
    )


def test_edit_line_appends_to_the_prefill():
    result = _drive(
        " via hash map\r",
        lambda: edit_line("? Commit message:  ", default="Add 1. Two Sum"),
    )
    assert result == "Add 1. Two Sum via hash map"


def test_edit_line_ctrl_u_clears():
    result = _drive(
        "\x15Rewritten\r",
        lambda: edit_line("? Commit message:  ", default="Add 1. Two Sum"),
    )
    assert result == "Rewritten"
