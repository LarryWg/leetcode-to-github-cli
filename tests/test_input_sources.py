from __future__ import annotations

import io
import subprocess
import sys

from lcpush import clipboard, editor


def test_read_stdin_until_eof_sentinel():
    stream = io.StringIO("line one\nline two\nEOF\nignored\n")
    assert editor.read_stdin(stream) == "line one\nline two\n"


def test_read_stdin_until_end_of_stream():
    assert editor.read_stdin(io.StringIO("a\nb\n")) == "a\nb\n"


def test_editor_command_prefers_visual(monkeypatch):
    monkeypatch.setenv("VISUAL", "code --wait")
    monkeypatch.setenv("EDITOR", "nano")
    assert editor.editor_command() == ["code", "--wait"]


def test_editor_command_falls_back_to_vi(monkeypatch):
    monkeypatch.delenv("VISUAL", raising=False)
    monkeypatch.delenv("EDITOR", raising=False)
    assert editor.editor_command() == ["vi"]


def test_strip_header_removes_the_instruction_line():
    body = editor.SOLUTION_HEADER + "print(1)\n"
    assert editor.strip_header(body, editor.SOLUTION_HEADER).strip() == "print(1)"


def test_open_editor_returns_none_when_unchanged(monkeypatch):
    monkeypatch.setenv("EDITOR", "true")
    monkeypatch.setattr(subprocess, "run", lambda *a, **k: None)
    assert editor.open_editor() is None


def test_open_editor_returns_written_content(monkeypatch):
    monkeypatch.setenv("EDITOR", "fake")

    def fake_run(command, **_kwargs):
        path = command[-1]
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(editor.SOLUTION_HEADER + "class Solution: pass\n")

    monkeypatch.setattr(subprocess, "run", fake_run)
    assert editor.open_editor().strip() == "class Solution: pass"


def test_open_editor_returns_none_for_empty_buffer(monkeypatch):
    monkeypatch.setenv("EDITOR", "fake")

    def fake_run(command, **_kwargs):
        with open(command[-1], "w", encoding="utf-8") as handle:
            handle.write("\n\n")

    monkeypatch.setattr(subprocess, "run", fake_run)
    assert editor.open_editor() is None


def test_clipboard_command_on_macos(monkeypatch):
    monkeypatch.setattr(sys, "platform", "darwin")
    monkeypatch.setattr(clipboard.shutil, "which", lambda name: "/usr/bin/pbpaste")
    assert clipboard.clipboard_command() == ["pbpaste"]


def test_clipboard_command_missing_tool(monkeypatch):
    monkeypatch.setattr(sys, "platform", "linux")
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)
    monkeypatch.setattr(clipboard.shutil, "which", lambda _name: None)
    assert clipboard.clipboard_command() is None
    assert not clipboard.available()
    assert clipboard.read() is None


def test_clipboard_prefers_wayland(monkeypatch):
    monkeypatch.setattr(sys, "platform", "linux")
    monkeypatch.setenv("WAYLAND_DISPLAY", "wayland-0")
    monkeypatch.setattr(clipboard.shutil, "which", lambda name: f"/usr/bin/{name}")
    assert clipboard.clipboard_command()[0] == "wl-paste"


def test_clipboard_read_returns_none_when_whitespace(monkeypatch):
    monkeypatch.setattr(clipboard, "clipboard_command", lambda: ["echo"])

    class Result:
        returncode = 0
        stdout = "   \n"

    monkeypatch.setattr(clipboard.subprocess, "run", lambda *a, **k: Result())
    assert clipboard.read() is None


def test_clipboard_read_returns_content(monkeypatch):
    monkeypatch.setattr(clipboard, "clipboard_command", lambda: ["echo"])

    class Result:
        returncode = 0
        stdout = "class Solution: pass\n"

    monkeypatch.setattr(clipboard.subprocess, "run", lambda *a, **k: Result())
    assert clipboard.read() == "class Solution: pass\n"


def test_clipboard_read_survives_a_failing_tool(monkeypatch):
    monkeypatch.setattr(clipboard, "clipboard_command", lambda: ["nope"])

    def boom(*_args, **_kwargs):
        raise OSError("not found")

    monkeypatch.setattr(clipboard.subprocess, "run", boom)
    assert clipboard.read() is None
