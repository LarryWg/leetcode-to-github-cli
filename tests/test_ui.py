from __future__ import annotations

from lcpush.ui import preview_lines, render_preview, render_push_panel
from tests.conftest import PY_SOLUTION

LONG = "".join(f"line {index}\n" for index in range(1, 25))


def test_preview_shows_head_and_tail_with_hidden_count():
    lines = preview_lines(LONG)
    assert lines[:5] == ["line 1", "line 2", "line 3", "line 4", "line 5"]
    assert lines[5] == " … 16 lines hidden …"
    assert lines[-3:] == ["line 22", "line 23", "line 24"]


def test_short_content_is_shown_whole():
    assert preview_lines("a\nb\nc\n") == ["a", "b", "c"]


def test_preview_header_carries_counts_and_language():
    box = render_preview("Clipboard", PY_SOLUTION, "Python3")
    assert "┌ Clipboard — 8 lines," in box
    assert "bytes, detected Python3" in box
    assert "class Solution:" in box


def test_preview_header_says_unknown_when_undetected():
    assert "detected unknown" in render_preview("Clipboard", "hello there\n", "unknown")


def test_push_panel_add():
    panel = render_push_panel(
        filename="0001-two-sum.py",
        lines=24,
        repo="user/leetcode-solutions",
        branch="main",
        message="Add 1. Two Sum (Python3)",
        updating=False,
    )
    assert "┌ Ready to push" in panel
    assert "overwrites" not in panel
    assert "File     0001-two-sum.py  (24 lines)" in panel
    assert "Repo     user/leetcode-solutions  (main)" in panel
    assert "Message  Add 1. Two Sum (Python3)" in panel
    assert "[Enter] push" in panel and "[m] edit message" in panel and "[n] cancel" in panel


def test_push_panel_overwrite_header_replaces_second_prompt():
    panel = render_push_panel(
        filename="0001-two-sum.py",
        lines=24,
        repo="user/solutions",
        branch="main",
        message="Update 1. Two Sum (Python3)",
        updating=True,
    )
    assert "┌ Ready to push (overwrites existing file)" in panel


def test_push_panel_hides_edit_keys_when_prompt_never():
    panel = render_push_panel(
        filename="f.py",
        lines=1,
        repo="user/solutions",
        branch="main",
        message="m",
        updating=False,
        prompt_mode="never",
    )
    assert "[m] edit message" not in panel
    assert "[Enter] push" in panel


def test_push_panel_renders_multiline_body():
    panel = render_push_panel(
        filename="f.py",
        lines=1,
        repo="user/solutions",
        branch="main",
        message="subject\n\nbody detail",
        updating=False,
    )
    assert "Message  subject" in panel
    assert "body detail" in panel
