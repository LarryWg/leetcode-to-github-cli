"""End-to-end CLI behaviour, focused on the non-interactive path (spec §6.7, §11)."""

from __future__ import annotations

import pytest
from typer.testing import CliRunner

from lcpush import session
from lcpush.cli import app
from lcpush.config import Config, save, set_value
from lcpush.github import PushResult
from lcpush.problems import save_cache
from tests.conftest import PY_SOLUTION

runner = CliRunner()


class FakeGitHub:
    """Records what the session would have sent to GitHub."""

    calls: list[dict] = []
    existing_sha: str | None = None

    def __init__(self, token, client=None):
        self.token = token

    def __enter__(self):
        return self

    def __exit__(self, *_exc):
        return None

    def get_file_sha(self, owner, name, path, branch):
        FakeGitHub.calls.append(
            {"op": "get", "owner": owner, "name": name, "path": path, "branch": branch}
        )
        return FakeGitHub.existing_sha

    def put_file(self, owner, name, path, **kwargs):
        FakeGitHub.calls.append({"op": "put", "path": path, **kwargs})
        return PushResult(
            html_url=f"https://github.com/{owner}/{name}/blob/{kwargs['branch']}/{path}",
            commit_sha="abc",
            updated=bool(kwargs.get("sha")),
        )


@pytest.fixture
def ready(monkeypatch, questions):
    """A configured, token-bearing, warm-cache environment with a fake GitHub."""
    save(set_value(Config(), "repo", "user/leetcode-solutions"))
    save_cache(questions)
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "ghp_test_token_value")
    FakeGitHub.calls = []
    FakeGitHub.existing_sha = None
    monkeypatch.setattr(session, "GitHubClient", FakeGitHub)
    return FakeGitHub


def _output(result) -> str:
    try:
        return result.output + result.stderr
    except ValueError:  # stderr not separately captured
        return result.output


def test_version():
    result = runner.invoke(app, ["--version"])
    assert result.exit_code == 0
    assert "lcpush" in result.output


def test_piped_push_completes_with_zero_prompts(ready):
    result = runner.invoke(
        app,
        ["--slug", "two-sum", "--lang", "python3", "--force"],
        input=PY_SOLUTION,
    )
    assert result.exit_code == 0, _output(result)

    put = [call for call in FakeGitHub.calls if call["op"] == "put"][0]
    assert put["path"] == "0001-two-sum.py"
    assert put["content"] == PY_SOLUTION
    assert put["message"] == "Add 1. Two Sum (Python3)"
    assert put["branch"] == "main"
    assert put["sha"] is None
    assert "blob/main/0001-two-sum.py" in _output(result)


def test_piped_without_force_is_refused(ready):
    result = runner.invoke(app, ["--slug", "two-sum", "--lang", "python3"], input=PY_SOLUTION)
    assert result.exit_code == 1
    assert "--force" in _output(result)


def test_custom_message_is_used_verbatim(ready):
    result = runner.invoke(
        app,
        ["--slug", "two-sum", "--lang", "python3", "--force", "-m", "Solve Two Sum with a hash map"],
        input=PY_SOLUTION,
    )
    assert result.exit_code == 0, _output(result)
    put = [call for call in FakeGitHub.calls if call["op"] == "put"][0]
    assert put["message"] == "Solve Two Sum with a hash map"


def test_overwrite_uses_the_update_template(ready):
    FakeGitHub.existing_sha = "existing-sha"
    result = runner.invoke(
        app, ["--slug", "two-sum", "--lang", "python3", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 0, _output(result)
    put = [call for call in FakeGitHub.calls if call["op"] == "put"][0]
    assert put["message"] == "Update 1. Two Sum (Python3)"
    assert put["sha"] == "existing-sha"


def test_no_clobber_aborts_instead_of_overwriting(ready):
    FakeGitHub.existing_sha = "existing-sha"
    result = runner.invoke(
        app,
        ["--slug", "two-sum", "--lang", "python3", "--force", "--no-clobber"],
        input=PY_SOLUTION,
    )
    assert result.exit_code == 1
    assert "--no-clobber" in _output(result)
    assert not [call for call in FakeGitHub.calls if call["op"] == "put"]


def test_path_prefix_is_applied(ready):
    save(set_value(set_value(Config(), "repo", "user/leetcode-solutions"), "path", "solutions"))
    result = runner.invoke(
        app, ["--slug", "two-sum", "--lang", "python3", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 0, _output(result)
    put = [call for call in FakeGitHub.calls if call["op"] == "put"][0]
    assert put["path"] == "solutions/0001-two-sum.py"


def test_language_is_detected_when_not_given(ready):
    result = runner.invoke(app, ["--slug", "two-sum", "--force"], input=PY_SOLUTION)
    assert result.exit_code == 0, _output(result)
    put = [call for call in FakeGitHub.calls if call["op"] == "put"][0]
    assert put["path"] == "0001-two-sum.py"


def test_undetectable_language_without_lang_flag_fails_clearly(ready):
    result = runner.invoke(app, ["--slug", "two-sum", "--force"], input="lorem ipsum dolor\n")
    assert result.exit_code == 1
    assert "--lang" in _output(result)


def test_soft_warnings_still_print_when_forced(ready):
    wrong = "var lengthOfLongestSubstring = function(s) {\n    return 0;\n};\n"
    result = runner.invoke(app, ["--slug", "two-sum", "--force"], input=wrong)
    assert result.exit_code == 0, _output(result)
    assert "Wrong question?" in _output(result)


def test_empty_stdin_is_rejected(ready):
    result = runner.invoke(app, ["--slug", "two-sum", "--lang", "python3", "--force"], input="   \n")
    assert result.exit_code == 1
    assert "empty" in _output(result).lower()


def test_unknown_slug_fails_clearly(ready):
    result = runner.invoke(
        app, ["--slug", "not-a-real-problem", "--lang", "python3", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 1
    assert "not-a-real-problem" in _output(result)


def test_unknown_language_flag_fails_clearly(ready):
    result = runner.invoke(
        app, ["--slug", "two-sum", "--lang", "cobol", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 1
    assert "cobol" in _output(result)


def test_id_flag_selects_the_question(ready):
    result = runner.invoke(
        app, ["--id", "167", "--lang", "python3", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 0, _output(result)
    put = [call for call in FakeGitHub.calls if call["op"] == "put"][0]
    assert put["path"] == "0167-two-sum-ii-input-array-is-sorted.py"


def test_repo_override_is_not_persisted(ready):
    from lcpush.config import load

    result = runner.invoke(
        app,
        ["--repo", "other/repo", "--slug", "two-sum", "--lang", "python3", "--force"],
        input=PY_SOLUTION,
    )
    assert result.exit_code == 0, _output(result)
    assert FakeGitHub.calls[0]["owner"] == "other"
    assert load().repo.full_name == "user/leetcode-solutions"


def test_config_show_redacts_the_token(ready):
    result = runner.invoke(app, ["config", "show"])
    assert result.exit_code == 0
    assert "ghp_test_token_value" not in result.output
    assert "redacted" in result.output
    assert "leetcode-solutions" in result.output


def test_config_show_without_config():
    result = runner.invoke(app, ["config", "show"])
    assert result.exit_code == 1
    assert "No config yet" in _output(result)


def test_config_set_branch(ready):
    from lcpush.config import load

    result = runner.invoke(app, ["config", "set", "branch", "trunk"])
    assert result.exit_code == 0
    assert load().repo.branch == "trunk"


def test_config_set_rejects_unknown_key(ready):
    result = runner.invoke(app, ["config", "set", "nope", "x"])
    assert result.exit_code == 1
    assert "Unknown config key" in _output(result)


def test_config_reset_token_warns_about_env(ready):
    result = runner.invoke(app, ["config", "reset-token"])
    assert result.exit_code == 0
    assert "LCPUSH_GITHUB_TOKEN" in _output(result)


def test_config_path_prints_locations(ready):
    result = runner.invoke(app, ["config", "path"])
    assert result.exit_code == 0
    assert "config:" in result.output and "cache:" in result.output


def test_missing_config_without_repo_flag_fails_clearly(monkeypatch, questions):
    save_cache(questions)
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "ghp_test_token_value")
    result = runner.invoke(
        app, ["--slug", "two-sum", "--lang", "python3", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 1
    assert "--repo" in _output(result)


def test_missing_token_fails_clearly(monkeypatch, questions):
    save(set_value(Config(), "repo", "user/leetcode-solutions"))
    save_cache(questions)
    monkeypatch.setattr("lcpush.tokens._keyring", lambda: None)
    monkeypatch.setattr("lcpush.tokens.shutil.which", lambda _name: None)
    result = runner.invoke(
        app, ["--slug", "two-sum", "--lang", "python3", "--force"], input=PY_SOLUTION
    )
    assert result.exit_code == 1
    assert "No GitHub token found" in _output(result)
