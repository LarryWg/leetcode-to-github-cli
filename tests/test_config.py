from __future__ import annotations

import os
import stat

import pytest

from lcpush import paths
from lcpush.config import (
    Config,
    from_dict,
    load,
    parse_repo,
    render_show,
    save,
    set_value,
)
from lcpush.errors import ConfigError


def test_defaults_match_spec():
    config = Config()
    assert config.commit.message_template == "Add {id}. {title} ({language})"
    assert config.commit.update_template == "Update {id}. {title} ({language})"
    assert config.commit.prompt == "confirm"
    assert config.cache.problems_ttl_days == 7
    assert config.repo.branch == "main"


@pytest.mark.parametrize(
    "value",
    [
        "user/leetcode-solutions",
        "https://github.com/user/leetcode-solutions",
        "git@github.com:user/leetcode-solutions.git",
        "  user/leetcode-solutions/  ",
    ],
)
def test_parse_repo_accepts_common_forms(value):
    assert parse_repo(value) == ("user", "leetcode-solutions")


@pytest.mark.parametrize("value", ["user", "a/b/c", ""])
def test_parse_repo_rejects_junk(value):
    with pytest.raises(ConfigError):
        parse_repo(value)


def test_round_trip(tmp_path):
    config = set_value(Config(), "repo", "user/solutions")
    path = save(config)
    assert path == paths.config_file()
    assert load() == config


def test_config_file_is_0600():
    save(Config())
    mode = stat.S_IMODE(os.stat(paths.config_file()).st_mode)
    assert mode == 0o600


def test_load_returns_none_without_a_file():
    assert load() is None


def test_load_rejects_broken_toml():
    path = paths.config_file()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("this is not = = toml", encoding="utf-8")
    with pytest.raises(ConfigError):
        load()


def test_set_repo_and_branch_and_path():
    config = set_value(Config(), "repo", "me/solutions")
    config = set_value(config, "branch", "trunk")
    config = set_value(config, "path", "solutions")
    assert config.repo.full_name == "me/solutions"
    assert config.repo.branch == "trunk"
    assert config.repo.path == "solutions/"


def test_path_prefix_normalizes_slashes():
    assert set_value(Config(), "path", "/a/b/").repo.path == "a/b/"
    assert set_value(Config(), "path", "  ").repo.path == ""


def test_set_is_immutable():
    original = Config()
    updated = set_value(original, "branch", "dev")
    assert original.repo.branch == "main"
    assert updated.repo.branch == "dev"


def test_set_commit_prompt_validates():
    assert set_value(Config(), "commit.prompt", "always").commit.prompt == "always"
    with pytest.raises(ConfigError):
        set_value(Config(), "commit.prompt", "sometimes")


def test_set_rejects_unknown_key():
    with pytest.raises(ConfigError):
        set_value(Config(), "repo.owner", "me")


def test_set_rejects_empty_template_and_branch():
    with pytest.raises(ConfigError):
        set_value(Config(), "commit.message_template", "   ")
    with pytest.raises(ConfigError):
        set_value(Config(), "branch", " ")


def test_set_ttl_validates():
    assert set_value(Config(), "cache.problems_ttl_days", "30").cache.problems_ttl_days == 30
    with pytest.raises(ConfigError):
        set_value(Config(), "cache.problems_ttl_days", "soon")
    with pytest.raises(ConfigError):
        set_value(Config(), "cache.problems_ttl_days", "-1")


def test_from_dict_falls_back_on_garbage():
    config = from_dict({"cache": {"problems_ttl_days": "many"}, "commit": {"prompt": "x"}})
    assert config.cache.problems_ttl_days == 7
    assert config.commit.prompt == "confirm"


def test_show_never_contains_a_token():
    config = set_value(Config(), "repo", "user/solutions")
    output = render_show(config, token_present=True, token_source="keyring")
    assert "ghp_" not in output
    assert "redacted" in output
    assert "user" in output
