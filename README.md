# lcpush

Push a LeetCode solution to a GitHub repo in one interactive session, from any
working directory. No clone, no working tree, no `cd`.

```
$ lcpush

  ? Question:  two su
      1.  Two Sum                                    [Easy]
      167. Two Sum II - Input Array Is Sorted        [Medium]
      653. Two Sum IV - Input is a BST               [Easy]
      1099. Two Sum Less Than K                      [Easy] 🔒
    ↑/↓ to move, Enter to select

  ✓ 1. Two Sum

  ? Solution source:  [Clipboard]  Editor  Stdin
  ✓ Read 24 lines from clipboard

  ┌ Clipboard — 24 lines, 612 bytes, detected Python3
  │ class Solution:
  │     def twoSum(self, nums: List[int], target: int) -> List[int]:
  │  … 16 lines hidden …
  │         return []
  └ ? Use this? [Y/n]

  ┌ Ready to push
  │ File     0001-two-sum.py  (24 lines)
  │ Repo     user/leetcode-solutions  (main)
  │ Message  Add 1. Two Sum (Python3)
  └ ? [Enter] push   [m] edit message   [M] edit in $EDITOR   [n] cancel

  → Pushing 0001-two-sum.py to user/leetcode-solutions (main)
  ✓ https://github.com/user/leetcode-solutions/blob/main/0001-two-sum.py
```

## Install

```bash
pipx install lcpush
```

From a checkout:

```bash
uv venv && uv pip install -e ".[dev]"
```

## First run

```
? GitHub repo to push to:  user/leetcode-solutions
? GitHub token:            ****     # auto-detected from `gh auth token` if available
✓ Verified write access to user/leetcode-solutions
✓ Saved config to ~/.config/lcpush/config.toml
```

Every run after that goes straight to the question picker.

The token needs the `repo` scope (classic) or `contents: read & write`
(fine-grained). It is stored in your OS keyring and **never** written to
`config.toml`.

## Usage

```
lcpush                                   # full interactive session
lcpush --slug two-sum                    # skip the picker
lcpush --id 1                            # skip the picker
lcpush --lang python3                    # skip language detection
lcpush -m "Solve Two Sum with a hash map"  # commit message, used verbatim
lcpush --repo owner/other-repo           # one-shot repo override, not persisted
lcpush --editor                          # write the solution in $EDITOR
lcpush --stdin                           # read the solution from stdin
lcpush --no-clipboard                    # drop the clipboard source
lcpush --no-clobber                      # abort instead of overwriting
lcpush --refresh                         # re-fetch the LeetCode problem list
```

Non-interactive, for scripts — `--force` is required because it skips the
preview guard:

```bash
cat sol.py | lcpush --slug two-sum --lang python3 --force
```

### Config

```
lcpush config show                       # token is described, never printed
lcpush config set repo owner/name        # re-verifies write access
lcpush config set branch main
lcpush config set path solutions/
lcpush config set commit.message_template "Add {id}. {title} ({language})"
lcpush config set commit.prompt confirm  # confirm | always | never
lcpush config set cache.problems_ttl_days 7
lcpush config reset-token
lcpush config path                       # where config and cache live
```

Config lives at `$XDG_CONFIG_HOME/lcpush/config.toml` (default
`~/.config/lcpush/config.toml`, mode 0600). The problem-set cache lives at
`$XDG_CACHE_HOME/lcpush/problems.json` (default `~/.cache/lcpush/`).

Commit-template variables: `{id}` `{padded_id}` `{title}` `{slug}`
`{language}` `{ext}` `{difficulty}` `{filename}` `{path}` `{lines}`. A
misspelled variable warns once and falls back to the built-in default rather
than crashing.

## How it works

| Step | Behaviour |
|---|---|
| Question search | The whole public problem set is cached locally and fuzzy-matched offline (RapidFuzz). Typing never hits the network. |
| Solution source | Clipboard, `$EDITOR`, or stdin. The clipboard is scored for "looks like code" and only preselected when it passes. |
| Preview | First 5 and last 3 lines, plus line and byte counts — the tail is what makes a truncated paste visible. Declining returns to the source menu. |
| Warnings | Unknown language, entry point that doesn't match the chosen question, conflict markers/TODOs, unbalanced brackets. All non-blocking. |
| Language | Detected by weighted regex signals, with explicit C/C++, Java/C#, and JS/TS tie-breaks. Always confirmed. |
| Target path | `{path}{id:0>4}-{slug}{ext}` — e.g. `0001-two-sum.py`. |
| Push | `GET .../contents/{path}` to learn add-vs-overwrite, then `PUT`. A 409 re-fetches the sha once and retries. |

Content is normalized only: CRLF → LF, trailing whitespace stripped per line,
exactly one trailing newline. Nothing is reformatted.

## Development

```bash
uv run pytest                     # test suite
uv run pytest --cov=lcpush        # with coverage
```

Layout:

| Module | Responsibility |
|---|---|
| `cli.py` | Typer flags, `config` subcommands, top-level error handling |
| `session.py` | The run flow, start to push |
| `onboarding.py` | First-run setup and token resolution |
| `config.py` / `paths.py` / `tokens.py` | Persistence |
| `problems.py` / `search.py` / `picker.py` | Problem set, fuzzy match, picker UI |
| `clipboard.py` / `editor.py` | Solution sources |
| `detect.py` / `plausibility.py` / `solution.py` | Language detection, scoring, validation |
| `render.py` / `github.py` / `ui.py` | Paths, commit messages, API, terminal output |

## License

MIT
