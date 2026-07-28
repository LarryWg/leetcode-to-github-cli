from __future__ import annotations

from pathlib import Path

import pytest

from lcpush import paths


@pytest.fixture
def no_overrides(monkeypatch):
    monkeypatch.delenv("LCPUSH_CONFIG_DIR", raising=False)
    monkeypatch.delenv("LCPUSH_CACHE_DIR", raising=False)
    monkeypatch.delenv("XDG_CONFIG_HOME", raising=False)
    monkeypatch.delenv("XDG_CACHE_HOME", raising=False)
    monkeypatch.setattr(paths.sys, "platform", "linux")


def test_defaults_to_dot_config_and_dot_cache(no_overrides, monkeypatch, tmp_path):
    monkeypatch.setattr(Path, "home", classmethod(lambda _cls: tmp_path))
    assert paths.config_dir() == tmp_path / ".config" / "lcpush"
    assert paths.cache_dir() == tmp_path / ".cache" / "lcpush"


def test_honours_xdg_variables(no_overrides, monkeypatch, tmp_path):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "cfg"))
    monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path / "cch"))
    assert paths.config_dir() == tmp_path / "cfg" / "lcpush"
    assert paths.cache_dir() == tmp_path / "cch" / "lcpush"


def test_explicit_overrides_win(monkeypatch, tmp_path):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "ignored"))
    monkeypatch.setenv("LCPUSH_CONFIG_DIR", str(tmp_path / "explicit"))
    assert paths.config_dir() == tmp_path / "explicit"


def test_file_names(tmp_path, monkeypatch):
    monkeypatch.setenv("LCPUSH_CONFIG_DIR", str(tmp_path / "c"))
    monkeypatch.setenv("LCPUSH_CACHE_DIR", str(tmp_path / "k"))
    assert paths.config_file().name == "config.toml"
    assert paths.token_file().name == "token"
    assert paths.problems_cache_file() == tmp_path / "k" / "problems.json"


def test_write_private_creates_parents(tmp_path):
    target = tmp_path / "deep" / "nested" / "file"
    paths.write_private(target, "secret\n")
    assert target.read_text() == "secret\n"


def test_write_private_truncates_an_existing_file(tmp_path):
    target = tmp_path / "file"
    paths.write_private(target, "a long previous value\n")
    paths.write_private(target, "new\n")
    assert target.read_text() == "new\n"
