# lcpush

**Push a LeetCode solution to GitHub without leaving your terminal.**

![Python](https://img.shields.io/badge/python-3.11%2B-blue)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey)

No clone. No working tree. No `cd`. Type `lcpush` from wherever you happen to
be, answer three prompts, and your solution is committed.

---

## Why I built this

Solving the problem was never the annoying part. The annoying part was
everything after: find the solutions repo, `cd` into it, remember whether it's
`0001-two-sum.py` or `1-two-sum.py`, paste, `git add`, write a commit message,
push. Thirty seconds of ceremony per problem, every time — enough friction that
I'd batch it up "for later" and then never do it.

So `lcpush` does the ceremony. It keeps the whole LeetCode problem set cached
locally so the question picker is instant, figures out the language from the
code itself, names the file the way it should be named, and pushes through the
GitHub API. There's no repo on disk to keep in sync, because there's no repo on
disk at all.

## Demo

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
  │         seen = {}
  │  … 18 lines hidden …
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

Not on PyPI yet, so install from source:

```bash
pipx install git+https://github.com/LarryWg/leetcode-to-github
```

Or from a local checkout:

```bash
pipx install .                          # puts `lcpush` on your PATH
uv venv && uv pip install -e ".[dev]"   # development, with the test suite
```

Requires Python 3.11+.

## First run

```
? GitHub repo to push to:  user/leetcode-solutions
? GitHub token:            ****     # auto-detected from `gh auth token` if available
✓ Verified write access to user/leetcode-solutions
✓ Saved config to ~/.config/lcpush/config.toml
```

That's the only time you'll be asked. Every run after this goes straight to the
question picker.

The token needs the **`repo`** scope (classic) or **`contents: read & write`**
(fine-grained). It's stored in your OS keyring — macOS Keychain, libsecret, or
Windows Credential Manager — and never written to `config.toml`. If no keyring
is available it falls back to a `0600` file and tells you it did.

## Usage

```bash
lcpush                                     # the full interactive session
lcpush --slug two-sum                      # skip the picker
lcpush --id 1                              # skip the picker
lcpush --lang python3                      # skip language detection
lcpush -m "Solve Two Sum with a hash map"  # commit message, used verbatim
lcpush --repo owner/other-repo             # one-shot override, not persisted
lcpush --editor                            # write the solution in $EDITOR
lcpush --stdin                             # read the solution from stdin
lcpush --no-clipboard                      # drop the clipboard source
lcpush --no-clobber                        # abort instead of overwriting
lcpush --refresh                           # re-fetch the problem list
```

Scriptable, with zero prompts:

```bash
cat sol.py | lcpush --slug two-sum --lang python3 --force
```

`--force` is mandatory for non-interactive runs. That's deliberate — it skips
the preview, and skipping the preview should be something you asked for out
loud rather than something that happens because stdin wasn't a terminal.

### Configuration

```bash
lcpush config show          # token is described, never printed
lcpush config set repo owner/name
lcpush config set branch main
lcpush config set path solutions/
lcpush config set commit.message_template "Add {id}. {title} ({language})"
lcpush config set commit.prompt confirm    # confirm | always | never
lcpush config set cache.problems_ttl_days 7
lcpush config reset-token
lcpush config path          # where config and cache live
```

Config lives at `$XDG_CONFIG_HOME/lcpush/config.toml` (default
`~/.config/lcpush/config.toml`, mode `0600`); the problem-set cache lives at
`$XDG_CACHE_HOME/lcpush/problems.json`.

Template variables: `{id}` `{padded_id}` `{title}` `{slug}` `{language}`
`{ext}` `{difficulty}` `{filename}` `{path}` `{lines}`. Misspell one and it
warns once and falls back to the built-in default — a typo in your config
shouldn't take the tool down.


### Field note: LeetCode's pagination lies

The GraphQL endpoint happily accepts `limit: 500` and returns `200 OK` — and
then gives you 100 results. If you paginate by advancing `skip` by the limit
you *requested*, you silently skip 400 problems per page and end up with 803
of the 4003 that exist, with no error anywhere to tell you. `lcpush` advances
by the length of the page it actually got back. There's a regression test
pinned to that behaviour, because I'd rather not rediscover it.

## How it fits together

| Module | Responsibility |
|---|---|
| `cli.py` | Typer flags, `config` subcommands, top-level error handling |
| `session.py` | The run flow, question to push |
| `onboarding.py` | First-run setup, token resolution |
| `config.py` `paths.py` `tokens.py` | Persistence, XDG paths, keyring |
| `problems.py` `search.py` `picker.py` | Problem set, offline fuzzy match, picker UI |
| `clipboard.py` `editor.py` | Solution sources |
| `detect.py` `plausibility.py` `solution.py` | Language detection, scoring, validation |
| `render.py` `github.py` `ui.py` | Paths, commit messages, API, terminal output |

## Development

```bash
uv venv
uv pip install -e ".[dev]"
uv run pytest                    # 240 tests
uv run pytest --cov=lcpush       # ~94% coverage
```

The prompt widgets are tested for real, driven through a `prompt_toolkit` pipe
input rather than mocked out — so "typing `two su` filters to Two Sum" and
"Ctrl-U clears the pre-filled commit message" are assertions, not hopes.

## Not doing (yet)

Deliberately out of scope for v1, in rough order of how likely I am to change
my mind:

- An auto-generated `README.md` index table in the solutions repo
- Grouping by difficulty or topic tag (the `path` config field is the seam)
- `lcpush log` — local history of what's been pushed
- Complexity/approach notes appended as a header comment
- Multiple languages for one question in a single invocation

Not planned: LeetCode account auth, scraping your accepted submissions, or any
host that isn't GitHub.
