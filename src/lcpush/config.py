"""Config file model and TOML persistence (spec §4).

The config is an immutable value object: every mutation returns a new `Config`
via `dataclasses.replace`, so a failed `config set` can never leave a
half-applied config behind. The token is *never* part of this model — see
`lcpush.tokens`.
"""

from __future__ import annotations

import tomllib
from dataclasses import dataclass, replace
from pathlib import Path

import tomli_w

from .errors import ConfigError
from .paths import config_file, write_private

DEFAULT_MESSAGE_TEMPLATE = "Add {id}. {title} ({language})"
DEFAULT_UPDATE_TEMPLATE = "Update {id}. {title} ({language})"
PROMPT_MODES = ("confirm", "always", "never")


@dataclass(frozen=True)
class RepoConfig:
    owner: str = ""
    name: str = ""
    branch: str = "main"
    path: str = ""

    @property
    def full_name(self) -> str:
        return f"{self.owner}/{self.name}"

    @property
    def configured(self) -> bool:
        return bool(self.owner and self.name)


@dataclass(frozen=True)
class CommitConfig:
    message_template: str = DEFAULT_MESSAGE_TEMPLATE
    update_template: str = DEFAULT_UPDATE_TEMPLATE
    prompt: str = "confirm"
    author_name: str = ""
    author_email: str = ""


@dataclass(frozen=True)
class CacheConfig:
    problems_ttl_days: int = 7


@dataclass(frozen=True)
class Config:
    repo: RepoConfig = RepoConfig()
    commit: CommitConfig = CommitConfig()
    cache: CacheConfig = CacheConfig()


def parse_repo(value: str) -> tuple[str, str]:
    """Parse `owner/name` (also accepting a full GitHub URL)."""
    text = value.strip().rstrip("/")
    for prefix in ("https://github.com/", "http://github.com/", "git@github.com:"):
        if text.startswith(prefix):
            text = text[len(prefix) :]
    if text.endswith(".git"):
        text = text[: -len(".git")]
    parts = [part for part in text.split("/") if part]
    if len(parts) != 2:
        raise ConfigError(f"Expected repo as owner/name, got: {value}")
    return parts[0], parts[1]


def normalize_path_prefix(value: str) -> str:
    """A non-empty subdirectory prefix always ends in exactly one slash."""
    text = value.strip().strip("/")
    return f"{text}/" if text else ""


def from_dict(data: dict) -> Config:
    repo_raw = data.get("repo") or {}
    commit_raw = data.get("commit") or {}
    cache_raw = data.get("cache") or {}
    try:
        ttl = int(cache_raw.get("problems_ttl_days", CacheConfig.problems_ttl_days))
    except (TypeError, ValueError):
        ttl = CacheConfig.problems_ttl_days
    prompt = str(commit_raw.get("prompt", CommitConfig.prompt))
    if prompt not in PROMPT_MODES:
        prompt = CommitConfig.prompt
    return Config(
        repo=RepoConfig(
            owner=str(repo_raw.get("owner", "")),
            name=str(repo_raw.get("name", "")),
            branch=str(repo_raw.get("branch", "") or "main"),
            path=normalize_path_prefix(str(repo_raw.get("path", ""))),
        ),
        commit=CommitConfig(
            message_template=str(
                commit_raw.get("message_template", DEFAULT_MESSAGE_TEMPLATE)
            ),
            update_template=str(
                commit_raw.get("update_template", DEFAULT_UPDATE_TEMPLATE)
            ),
            prompt=prompt,
            author_name=str(commit_raw.get("author_name", "")),
            author_email=str(commit_raw.get("author_email", "")),
        ),
        cache=CacheConfig(problems_ttl_days=ttl),
    )


def to_dict(config: Config) -> dict:
    return {
        "repo": {
            "owner": config.repo.owner,
            "name": config.repo.name,
            "branch": config.repo.branch,
            "path": config.repo.path,
        },
        "commit": {
            "message_template": config.commit.message_template,
            "update_template": config.commit.update_template,
            "prompt": config.commit.prompt,
            "author_name": config.commit.author_name,
            "author_email": config.commit.author_email,
        },
        "cache": {"problems_ttl_days": config.cache.problems_ttl_days},
    }


def load(path: Path | None = None) -> Config | None:
    """Return the stored config, or None when there is no config file yet."""
    target = path or config_file()
    if not target.exists():
        return None
    try:
        with target.open("rb") as handle:
            data = tomllib.load(handle)
    except (tomllib.TOMLDecodeError, OSError) as exc:
        raise ConfigError(f"Could not read config at {target}: {exc}") from exc
    return from_dict(data)


def save(config: Config, path: Path | None = None) -> Path:
    """Persist the config with mode 0600 and return where it was written."""
    target = path or config_file()
    write_private(target, tomli_w.dumps(to_dict(config)))
    return target


SETTABLE_KEYS = (
    "repo",
    "branch",
    "path",
    "commit.message_template",
    "commit.update_template",
    "commit.prompt",
    "commit.author_name",
    "commit.author_email",
    "cache.problems_ttl_days",
)


def set_value(config: Config, key: str, value: str) -> Config:
    """Return a new Config with `key` set to `value`.

    Raises ConfigError for unknown keys or invalid values; the caller's config
    object is untouched either way.
    """
    if key == "repo":
        owner, name = parse_repo(value)
        return replace(config, repo=replace(config.repo, owner=owner, name=name))
    if key == "branch":
        branch = value.strip()
        if not branch:
            raise ConfigError("Branch cannot be empty")
        return replace(config, repo=replace(config.repo, branch=branch))
    if key == "path":
        return replace(
            config, repo=replace(config.repo, path=normalize_path_prefix(value))
        )
    if key in ("commit.message_template", "commit.update_template"):
        field = key.split(".", 1)[1]
        if not value.strip():
            raise ConfigError(f"{key} cannot be empty")
        return replace(config, commit=replace(config.commit, **{field: value}))
    if key == "commit.prompt":
        mode = value.strip().lower()
        if mode not in PROMPT_MODES:
            raise ConfigError(
                f"commit.prompt must be one of {', '.join(PROMPT_MODES)}, got: {value}"
            )
        return replace(config, commit=replace(config.commit, prompt=mode))
    if key in ("commit.author_name", "commit.author_email"):
        field = key.split(".", 1)[1]
        return replace(config, commit=replace(config.commit, **{field: value.strip()}))
    if key == "cache.problems_ttl_days":
        try:
            days = int(value)
        except ValueError as exc:
            raise ConfigError(f"cache.problems_ttl_days must be an integer: {value}") from exc
        if days < 0:
            raise ConfigError("cache.problems_ttl_days must be >= 0")
        return replace(config, cache=replace(config.cache, problems_ttl_days=days))
    raise ConfigError(
        f"Unknown config key: {key}. Known keys: {', '.join(SETTABLE_KEYS)}"
    )


def render_show(config: Config, *, token_present: bool, token_source: str = "") -> str:
    """Human-readable config dump. The token is described, never printed."""
    doc = tomli_w.dumps(to_dict(config))
    status = f"set (source: {token_source})" if token_present else "not set"
    return f"{doc}\n[token]\nstatus = \"{status}\"  # value redacted, never stored in config.toml"
