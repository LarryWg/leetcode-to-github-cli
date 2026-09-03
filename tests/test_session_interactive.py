"""The interactive flow, driven by scripted prompt answers (spec §3, §6.3, §8.3)."""

from __future__ import annotations

import pytest

from lcpush import session
from lcpush.config import Config, save, set_value
from lcpush.errors import Cancelled
from lcpush.github import PushResult
from lcpush.problems import save_cache
from tests.conftest import CPP_SOLUTION, PY_SOLUTION

URL_ON_CLIPBOARD = "https://leetcode.com/problems/two-sum/"


class FakeStdin:
    @staticmethod
    def isatty() -> bool:
        return True


class Recorder:
    """Scripted answers for every interactive prompt, plus a call log."""

    def __init__(self, *, keys=(), confirms=(), selects=None, lines=()):
        self.keys = list(keys)
        self.confirms = list(confirms)
        self.selects = dict(selects or {})
        self.lines = list(lines)
        self.select_calls: list[tuple[str, list, object]] = []
        self.edit_defaults: list[str] = []
        self.panels: list[str] = []

    def select(self, message, entries, default=None):
        self.select_calls.append((message, entries, default))
        if message in self.selects:
            return self.selects[message]
        return default if default is not None else entries[0].value

    def confirm(self, _message, default=True):
        return self.confirms.pop(0) if self.confirms else default

    def read_key(self, _allowed):
        return self.keys.pop(0) if self.keys else "push"

    def edit_line(self, _message, *, default=""):
        self.edit_defaults.append(default)
        return self.lines.pop(0) if self.lines else default


class FakeGitHub:
    puts: list[dict] = []
    existing_sha: str | None = None

    def __init__(self, token, client=None):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *_exc):
        return None

    def get_file_sha(self, *_args):
        return FakeGitHub.existing_sha

    def put_file(self, owner, name, path, **kwargs):
        FakeGitHub.puts.append({"path": path, **kwargs})
        return PushResult(f"https://github.com/{owner}/{name}/blob/main/{path}", "sha", False)


@pytest.fixture
def interactive(monkeypatch, questions, capsys):
    save(set_value(Config(), "repo", "user/leetcode-solutions"))
    save_cache(questions)
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "ghp_test_token")
    monkeypatch.setattr("sys.stdin", FakeStdin())
    monkeypatch.setattr(session, "GitHubClient", FakeGitHub)
    monkeypatch.setattr(session, "pick", lambda _index: questions[0])
    monkeypatch.setattr(session.clipboard, "read", lambda: PY_SOLUTION)
    FakeGitHub.puts = []
    FakeGitHub.existing_sha = None

    def install(recorder: Recorder) -> Recorder:
        monkeypatch.setattr(session.prompts, "select", recorder.select)
        monkeypatch.setattr(session.prompts, "confirm", recorder.confirm)
        monkeypatch.setattr(session.prompts, "read_key", recorder.read_key)
        monkeypatch.setattr(session.prompts, "edit_line", recorder.edit_line)
        return recorder

    return install


def test_happy_path_clipboard_to_push(interactive, capsys):
    recorder = interactive(Recorder(confirms=[True], keys=["push"]))
    assert session.run() == 0

    put = FakeGitHub.puts[0]
    assert put["path"] == "0001-two-sum.py"
    assert put["content"] == PY_SOLUTION
    assert put["message"] == "Add 1. Two Sum (Python3)"

    out = capsys.readouterr().out
    assert "┌ Clipboard — 8 lines" in out
    assert "┌ Ready to push" in out
    assert "Message  Add 1. Two Sum (Python3)" in out
    assert "blob/main/0001-two-sum.py" in out
    assert recorder.edit_defaults == []  # Enter alone pushed it


def test_piped_stdin_is_refused(interactive, monkeypatch):
    class PipedStdin:
        @staticmethod
        def isatty() -> bool:
            return False

    monkeypatch.setattr("sys.stdin", PipedStdin())
    interactive(Recorder())
    with pytest.raises(session.InputError) as excinfo:
        session.run()
    assert "terminal" in excinfo.value.message


def test_clipboard_is_preselected_when_plausible(interactive):
    recorder = interactive(Recorder(confirms=[True], keys=["push"]))
    session.run()
    message, entries, default = recorder.select_calls[0]
    assert message == "? Solution source:"
    assert default == "clipboard"
    assert entries[0].title == "Clipboard"


def test_url_on_clipboard_is_labelled_and_not_preselected(interactive, monkeypatch):
    monkeypatch.setattr(session.clipboard, "read", lambda: URL_ON_CLIPBOARD)
    monkeypatch.setattr(
        session.editor, "open_editor", lambda **_kwargs: PY_SOLUTION
    )
    recorder = interactive(Recorder(confirms=[True], keys=["push"]))
    session.run()

    _message, entries, default = recorder.select_calls[0]
    assert default == "editor"
    assert entries[0].title == "Clipboard (doesn't look like code)"
    assert entries[0].value == "clipboard"  # still selectable


def test_declining_the_preview_returns_to_the_source_menu(interactive, monkeypatch):
    """Acceptance 10: `n` costs one keypress, not a restarted session."""
    monkeypatch.setattr(session.editor, "open_editor", lambda **_kwargs: PY_SOLUTION)
    recorder = interactive(
        Recorder(
            confirms=[False, True],
            keys=["push"],
            selects={},
        )
    )
    # First pass takes the clipboard and is declined; second pass picks Editor.
    answers = iter(["clipboard", "editor"])
    original_select = recorder.select

    def select(message, entries, default=None):
        result = original_select(message, entries, default)
        return next(answers) if message == "? Solution source:" else result

    monkeypatch.setattr(session.prompts, "select", select)

    assert session.run() == 0
    assert len(FakeGitHub.puts) == 1
    source_menus = [c for c in recorder.select_calls if c[0] == "? Solution source:"]
    assert len(source_menus) == 2  # the menu was shown again, not a new session


def test_pressing_m_prefills_the_rendered_message(interactive):
    """Acceptance 13: the edit field is pre-filled, not blank."""
    recorder = interactive(
        Recorder(
            confirms=[True],
            keys=["edit", "push"],
            lines=["Add 1. Two Sum (Python3) via hash map"],
        )
    )
    session.run()
    assert recorder.edit_defaults == ["Add 1. Two Sum (Python3)"]
    assert FakeGitHub.puts[0]["message"] == "Add 1. Two Sum (Python3) via hash map"


def test_empty_message_is_rejected_and_reprompted(interactive):
    recorder = interactive(
        Recorder(confirms=[True], keys=["edit", "push"], lines=["   ", "Real message"])
    )
    session.run()
    assert len(recorder.edit_defaults) == 2
    assert FakeGitHub.puts[0]["message"] == "Real message"


def test_n_cancels_without_pushing(interactive):
    interactive(Recorder(confirms=[True], keys=["cancel"]))
    with pytest.raises(Cancelled) as excinfo:
        session.run()
    assert FakeGitHub.puts == []
    assert excinfo.value.exit_code == 130


def test_overwrite_is_confirmed_by_the_panel_alone(interactive, capsys):
    """Acceptance 15: one confirmation, and the update template is used."""
    FakeGitHub.existing_sha = "existing"
    recorder = interactive(Recorder(confirms=[True], keys=["push"]))
    session.run()

    out = capsys.readouterr().out
    assert "┌ Ready to push (overwrites existing file)" in out
    assert "Message  Update 1. Two Sum (Python3)" in out
    assert FakeGitHub.puts[0]["sha"] == "existing"
    assert len(recorder.confirms) == 0  # only the preview confirm was consumed


def test_commit_prompt_never_hides_the_edit_keys(interactive, capsys):
    save(
        set_value(
            set_value(Config(), "repo", "user/leetcode-solutions"),
            "commit.prompt",
            "never",
        )
    )
    recorder = interactive(Recorder(confirms=[True], keys=["push"]))
    session.run()
    assert FakeGitHub.puts[0]["message"] == "Add 1. Two Sum (Python3)"
    assert recorder.edit_defaults == []
    assert "[m] edit message" not in capsys.readouterr().out


def test_commit_prompt_always_opens_the_message_field(interactive, monkeypatch):
    save(
        set_value(
            set_value(Config(), "repo", "user/leetcode-solutions"),
            "commit.prompt",
            "always",
        )
    )
    recorder = interactive(Recorder(confirms=[True], keys=["push"], lines=["Edited up front"]))
    session.run()
    assert recorder.edit_defaults == ["Add 1. Two Sum (Python3)"]
    assert FakeGitHub.puts[0]["message"] == "Edited up front"


def test_language_menu_preselects_the_detection(interactive):
    recorder = interactive(Recorder(confirms=[True], keys=["push"]))
    session.run()
    language_call = [c for c in recorder.select_calls if c[0] == "? Language:"][0]
    _message, entries, default = language_call
    assert default == "python3"
    assert entries[0].title == "Python3  (detected)"
    assert len(entries) == 14  # every supported language, ranked


def test_undetectable_language_preselects_nothing(interactive, monkeypatch):
    monkeypatch.setattr(session.clipboard, "read", lambda: "the quick brown fox jumps\n")
    recorder = interactive(
        Recorder(
            confirms=[True],
            keys=["push"],
            selects={"? Solution source:": "clipboard", "? Language:": "python3"},
        )
    )
    session.run()
    language_call = [c for c in recorder.select_calls if c[0] == "? Language:"][0]
    assert language_call[2] is None


def test_empty_editor_buffer_aborts(interactive, monkeypatch):
    """Spec §9: an editor that comes back empty is an abort, not a retry."""
    monkeypatch.setattr(session.editor, "open_editor", lambda **_kwargs: None)
    interactive(
        Recorder(confirms=[True], keys=["push"], selects={"? Solution source:": "editor"})
    )
    with pytest.raises(session.InputError) as excinfo:
        session.run()
    assert excinfo.value.message == "No solution provided. Aborted."
    assert FakeGitHub.puts == []


def test_empty_clipboard_at_read_time_returns_to_the_menu(interactive, monkeypatch):
    reads = {"n": 0}

    def read():
        reads["n"] += 1
        return PY_SOLUTION if reads["n"] > 2 else None

    monkeypatch.setattr(session.clipboard, "read", read)
    monkeypatch.setattr(session.editor, "open_editor", lambda **_kwargs: PY_SOLUTION)
    recorder = interactive(
        Recorder(confirms=[True], keys=["push"], selects={"? Solution source:": "clipboard"})
    )
    session.run()
    assert len([c for c in recorder.select_calls if c[0] == "? Solution source:"]) >= 2
    assert FakeGitHub.puts[0]["content"] == PY_SOLUTION


def test_truncated_paste_warns_and_shows_the_tail(interactive, monkeypatch, capsys):
    """Acceptance 11: unbalanced brackets plus a visible tail."""
    truncated = "\n".join(CPP_SOLUTION.split("\n")[:8]) + "\n"
    monkeypatch.setattr(session.clipboard, "read", lambda: truncated)
    interactive(Recorder(confirms=[True], keys=["push"]))
    session.run()
    captured = capsys.readouterr()
    assert "Brackets are unbalanced" in captured.err
    assert "8 lines" in captured.out  # a line count that gives the truncation away
    assert "seen[nums[i]] = i;" in captured.out  # ...and the tail that proves it


def test_token_source_is_announced_on_first_run(interactive, monkeypatch, capsys):
    from lcpush import onboarding
    from lcpush.github import RepoInfo
    from lcpush.paths import config_file

    config_file().unlink()
    monkeypatch.setattr(onboarding.prompts, "text", lambda *_a, **_k: "o/r")
    monkeypatch.setattr(
        onboarding, "verify_repo", lambda *_a, **_k: RepoInfo("o/r", "main", True)
    )
    interactive(Recorder(confirms=[True], keys=["push"]))
    assert session.run() == 0
    out = capsys.readouterr().out
    assert "Using GitHub token from $LCPUSH_GITHUB_TOKEN" in out
    assert "ghp_test_token" not in out


def test_wrong_question_warning_is_not_blocking(interactive, monkeypatch, capsys):
    monkeypatch.setattr(
        session.clipboard,
        "read",
        lambda: "var lengthOfLongestSubstring = function(s) {\n    return 0;\n};\n",
    )
    interactive(Recorder(confirms=[True], keys=["push"]))
    assert session.run() == 0
    assert 'but you selected "Two Sum"' in capsys.readouterr().err
