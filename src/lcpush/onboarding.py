"""First-run setup: repo, token, write-access verification (spec §3, §4)."""

from __future__ import annotations

from dataclasses import replace

import httpx

from . import prompts, tokens, ui
from .config import Config, parse_repo, save
from .errors import ConfigError, TokenError
from .github import GitHubClient
from .paths import token_file


def resolve_token(*, interactive: bool, announce: bool = False) -> str:
    """Find a token, prompting (hidden) as the last resort."""
    found = tokens.resolve()
    if found is not None:
        if announce:
            ui.dim(f"  Using GitHub token from {found.source}")
        return found.value
    if not interactive:
        raise TokenError(
            "No GitHub token found. Set $LCPUSH_GITHUB_TOKEN, or run lcpush "
            "interactively once to store one."
        )
    ui.dim("  No stored GitHub token found.")
    value = prompts.password("? GitHub token:").strip()
    if not value:
        raise TokenError("No GitHub token provided.")
    where = tokens.store(value)
    if where == "keyring":
        ui.success("Saved token to the OS keyring")
    else:
        ui.warn(
            f"⚠ No OS keyring available; stored the token at {token_file()} (mode 0600)."
        )
    return value


def verify_repo(token: str, owner: str, name: str, *, client: httpx.Client | None = None):
    with GitHubClient(token, client=client) as github:
        info = github.get_repo(owner, name)
    if not info.can_push:
        raise TokenError(
            f"Token cannot write to {owner}/{name}. Needs the `repo` scope "
            "(classic) or `contents: read & write` (fine-grained)."
        )
    return info


def setup(existing: Config | None = None) -> tuple[Config, str]:
    """Run the first-run flow and return the saved config plus the token."""
    config = existing or Config()

    ui.info("")
    answer = prompts.text("? GitHub repo to push to:", default="").strip()
    if not answer:
        raise ConfigError("A target repo is required.")
    owner, name = parse_repo(answer)

    token = resolve_token(interactive=True, announce=True)
    info = verify_repo(token, owner, name)
    ui.success(f"Verified write access to {owner}/{name}")

    config = replace(
        config,
        repo=replace(
            config.repo, owner=owner, name=name, branch=info.default_branch
        ),
    )
    path = save(config)
    ui.success(f"Saved config to {path}")
    return config, token
