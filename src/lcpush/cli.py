"""Typer entry point: flags, `config` subcommands, top-level error handling."""

from __future__ import annotations

import os
import sys
from typing import Annotated

import typer

from . import __version__, tokens, ui
from .config import Config, SETTABLE_KEYS, load, parse_repo, render_show, save, set_value
from .errors import Cancelled, ConfigError, LcpushError
from .onboarding import resolve_token, verify_repo
from .session import run

app = typer.Typer(
    add_completion=False,
    no_args_is_help=False,
    context_settings={"help_option_names": ["-h", "--help"]},
    help="Push a LeetCode solution to a GitHub repo in one interactive session.",
)
config_app = typer.Typer(help="Inspect and change lcpush configuration.")
app.add_typer(config_app, name="config")


def _fail(exc: LcpushError) -> None:
    ui.error(exc.message)
    raise typer.Exit(exc.exit_code)


@app.callback(invoke_without_command=True)
def main(
    ctx: typer.Context,
    refresh: Annotated[
        bool, typer.Option("--refresh", help="Re-fetch the LeetCode problem list.")
    ] = False,
    version: Annotated[
        bool, typer.Option("--version", help="Print the version and exit.")
    ] = False,
) -> None:
    if version:
        typer.echo(f"lcpush {__version__}")
        raise typer.Exit(0)
    if ctx.invoked_subcommand is not None:
        return

    try:
        code = run(refresh=refresh)
    except Cancelled as exc:
        ui.error(exc.message)
        raise typer.Exit(exc.exit_code) from exc
    except KeyboardInterrupt:
        ui.error("Aborted.")
        raise typer.Exit(130) from None
    except LcpushError as exc:
        _fail(exc)
        return
    raise typer.Exit(code)


def _require_config() -> Config:
    config = load()
    if config is None:
        raise ConfigError("No config yet. Run lcpush once to set up a repo.")
    return config


@config_app.command("show")
def config_show() -> None:
    """Print the current config; the token is described, never printed."""
    try:
        config = _require_config()
    except LcpushError as exc:
        _fail(exc)
        return
    found = tokens.resolve()
    typer.echo(
        render_show(
            config,
            token_present=found is not None,
            token_source=found.source if found else "",
        )
    )


@config_app.command("set")
def config_set(
    key: Annotated[str, typer.Argument(help=f"One of: {', '.join(SETTABLE_KEYS)}")],
    value: Annotated[str, typer.Argument(help="New value.")],
) -> None:
    """Set a config key. `repo` re-verifies write access before saving."""
    try:
        config = load() or Config()
        updated = set_value(config, key, value)
        if key == "repo":
            owner, name = parse_repo(value)
            token = resolve_token(interactive=sys.stdin.isatty())
            info = verify_repo(token, owner, name)
            ui.success(f"Verified write access to {owner}/{name}")
            if not config.repo.configured:
                updated = set_value(updated, "branch", info.default_branch)
        path = save(updated)
        ui.success(f"Updated {key} in {path}")
    except Cancelled as exc:
        ui.error(exc.message)
        raise typer.Exit(exc.exit_code) from exc
    except LcpushError as exc:
        _fail(exc)


@config_app.command("reset-token")
def config_reset_token() -> None:
    """Forget the stored GitHub token (keyring and fallback file)."""
    cleared = tokens.clear()
    if cleared:
        ui.success(f"Cleared stored token from: {', '.join(cleared)}")
    else:
        ui.info("No stored token to clear.")
    leftover = [name for name in tokens.ENV_VARS if name in os.environ]
    if leftover:
        ui.warn(
            f"⚠ {', '.join('$' + name for name in leftover)} is still set in the "
            "environment and takes precedence."
        )


@config_app.command("path")
def config_path() -> None:
    """Print where lcpush keeps its config and cache."""
    from .paths import config_file, problems_cache_file

    typer.echo(f"config: {config_file()}")
    typer.echo(f"cache:  {problems_cache_file()}")


if __name__ == "__main__":  # pragma: no cover
    app()
