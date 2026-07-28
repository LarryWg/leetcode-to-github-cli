from __future__ import annotations

import os
import stat

from lcpush import tokens
from lcpush.paths import token_file


def test_env_var_precedence(monkeypatch):
    monkeypatch.setenv("GITHUB_TOKEN", "from-github-token")
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "from-lcpush-token")
    found = tokens.resolve()
    assert found.value == "from-lcpush-token"
    assert found.source == "$LCPUSH_GITHUB_TOKEN"


def test_env_fallback_to_github_token(monkeypatch):
    monkeypatch.setenv("GITHUB_TOKEN", "gh-token")
    assert tokens.from_env().source == "$GITHUB_TOKEN"


def test_blank_env_is_ignored(monkeypatch):
    monkeypatch.setenv("LCPUSH_GITHUB_TOKEN", "   ")
    assert tokens.from_env() is None


def test_file_fallback_is_0600(monkeypatch):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    where = tokens.store("secret-value")
    assert where == str(token_file())
    assert stat.S_IMODE(os.stat(token_file()).st_mode) == 0o600
    assert tokens.from_file().value == "secret-value"


def test_keyring_is_preferred(monkeypatch):
    store: dict[tuple[str, str], str] = {}

    class FakeKeyring:
        @staticmethod
        def set_password(service, account, value):
            store[(service, account)] = value

        @staticmethod
        def get_password(service, account):
            return store.get((service, account))

        @staticmethod
        def delete_password(service, account):
            store.pop((service, account), None)

    monkeypatch.setattr(tokens, "_keyring", lambda: FakeKeyring)
    assert tokens.store("kr-token") == "keyring"
    assert tokens.from_keyring().value == "kr-token"
    assert tokens.clear() == ["keyring"]
    assert tokens.from_keyring() is None


def test_keyring_failure_falls_back_to_file(monkeypatch):
    class BrokenKeyring:
        @staticmethod
        def set_password(*_args):
            raise RuntimeError("no backend")

        @staticmethod
        def get_password(*_args):
            raise RuntimeError("no backend")

    monkeypatch.setattr(tokens, "_keyring", lambda: BrokenKeyring)
    assert tokens.store("fallback") == str(token_file())
    assert tokens.from_keyring() is None


def test_gh_cli_used_when_present(monkeypatch):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    monkeypatch.setattr(tokens.shutil, "which", lambda _name: "/usr/bin/gh")

    class Result:
        returncode = 0
        stdout = "gho_from_cli\n"

    monkeypatch.setattr(tokens.subprocess, "run", lambda *a, **k: Result())
    found = tokens.resolve()
    assert found.value == "gho_from_cli"
    assert found.source == "gh auth token"


def test_gh_cli_absent(monkeypatch):
    monkeypatch.setattr(tokens.shutil, "which", lambda _name: None)
    assert tokens.from_gh_cli() is None


def test_resolve_returns_none_when_nothing_stored(monkeypatch):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    monkeypatch.setattr(tokens.shutil, "which", lambda _name: None)
    assert tokens.resolve() is None


def test_clear_removes_the_file(monkeypatch):
    monkeypatch.setattr(tokens, "_keyring", lambda: None)
    tokens.store("bye")
    assert tokens.clear() == [str(token_file())]
    assert not token_file().exists()


def test_redact():
    assert tokens.redact("url ghp_abcdefgh here", "ghp_abcdefgh") == "url **** here"
    assert tokens.redact("nothing", None) == "nothing"
    assert tokens.redact("short", "abc") == "short"
