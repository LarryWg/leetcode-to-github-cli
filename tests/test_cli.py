"""CLI surface behaviour: flags, `config` subcommands, error handling."""

from __future__ import annotations

import pytest
from typer.testing import CliRunner

from lcpush.cli import app
from lcpush.config import Config, save, set_value
from lcpush.problems import save_cache

runner = CliRunner()


@pytest.fixture
def ready(monkeypatch, questions):
    """A configured, token-bearing, warm-cache environment."""
    save(set_value(Config(), "repo", "user/leetcode-solutions"))
    save_cache(questions)
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "ghp_test_token_value")


def _output(result) -> str:
    try:
        return result.output + result.stderr
    except ValueError:  # stderr not separately captured
        return result.output


def test_version():
    result = runner.invoke(app, ["--version"])
    assert result.exit_code == 0
    assert "lcpush" in result.output


def test_piped_input_is_refused(ready):
    result = runner.invoke(app, [], input="print('hi')\n")
    assert result.exit_code == 1
    assert "terminal" in _output(result)


def test_unknown_flag_is_rejected(ready):
    result = runner.invoke(app, ["--slug", "two-sum"])
    assert result.exit_code != 0


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
