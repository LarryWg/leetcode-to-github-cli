from __future__ import annotations

import httpx
import pytest

from lcpush import onboarding, tokens
from lcpush.config import Config, load
from lcpush.errors import ConfigError, TokenError
from lcpush.github import RepoInfo
from lcpush.paths import token_file


def _repo_response(default_branch="main", push=True):
    def handler(_request):
        return httpx.Response(
            200,
            json={
                "full_name": "user/solutions",
                "default_branch": default_branch,
                "permissions": {"push": push},
            },
        )

    return httpx.Client(transport=httpx.MockTransport(handler))


def test_resolve_token_prefers_the_environment(monkeypatch):
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "env-token")
    assert onboarding.resolve_token(interactive=False) == "env-token"


def test_resolve_token_non_interactive_without_one(monkeypatch):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    monkeypatch.setattr(tokens.shutil, "which", lambda _name: None)
    with pytest.raises(TokenError) as excinfo:
        onboarding.resolve_token(interactive=False)
    assert "LCPUSH_GITHUB_TOKEN" in excinfo.value.message


def test_resolve_token_prompts_and_stores(monkeypatch, capsys):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    monkeypatch.setattr(tokens.shutil, "which", lambda _name: None)
    monkeypatch.setattr(onboarding.prompts, "password", lambda _message: " ghp_typed ")

    assert onboarding.resolve_token(interactive=True) == "ghp_typed"
    assert token_file().read_text().strip() == "ghp_typed"
    assert "No OS keyring available" in capsys.readouterr().err


def test_resolve_token_rejects_an_empty_prompt(monkeypatch):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    monkeypatch.setattr(tokens.shutil, "which", lambda _name: None)
    monkeypatch.setattr(onboarding.prompts, "password", lambda _message: "   ")
    with pytest.raises(TokenError):
        onboarding.resolve_token(interactive=True)


def test_verify_repo_rejects_read_only_tokens():
    with pytest.raises(TokenError) as excinfo:
        onboarding.verify_repo("t", "user", "solutions", client=_repo_response(push=False))
    assert "contents: read & write" in excinfo.value.message


def test_verify_repo_returns_the_default_branch():
    info = onboarding.verify_repo("t", "user", "solutions", client=_repo_response("trunk"))
    assert info.default_branch == "trunk"


def test_setup_persists_repo_and_resolved_branch(monkeypatch, capsys):
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "env-token")
    monkeypatch.setattr(onboarding.prompts, "text", lambda *_a, **_k: "user/solutions")
    monkeypatch.setattr(
        onboarding,
        "verify_repo",
        lambda *_args, **_kwargs: RepoInfo("user/solutions", "trunk", True),
    )

    config, token = onboarding.setup()

    assert token == "env-token"
    assert config.repo.full_name == "user/solutions"
    assert config.repo.branch == "trunk"
    assert load() == config

    out = capsys.readouterr().out
    assert "Verified write access to user/solutions" in out
    assert "Saved config to" in out
    assert "env-token" not in out


def test_setup_prompts_for_the_repo_interactively(monkeypatch):
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "env-token")
    monkeypatch.setattr(onboarding.prompts, "text", lambda *_a, **_k: "user/solutions")
    monkeypatch.setattr(
        onboarding,
        "verify_repo",
        lambda *_args, **_kwargs: RepoInfo("user/solutions", "main", True),
    )
    config, _token = onboarding.setup(Config())
    assert config.repo.full_name == "user/solutions"


def test_setup_requires_a_repo(monkeypatch):
    monkeypatch.setattr(onboarding.prompts, "text", lambda *_a, **_k: "  ")
    with pytest.raises(ConfigError):
        onboarding.setup(Config())
