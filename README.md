# lcpush

**Push a LeetCode solution to GitHub without leaving your terminal.**

![C++](https://img.shields.io/badge/C%2B%2B-20-blue)
![License](https://img.shields.io/badge/license-MIT-blue)
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

Build from source with CMake (3.24+), a C++20 compiler, and libcurl
(preinstalled on macOS, `libcurl4-openssl-dev` or similar on Linux). All other
dependencies are pinned and fetched at configure time.

```bash
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build       # puts `lcpush` on your PATH (may need sudo)
```

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
(fine-grained). It's stored in the macOS
Keychain and never written to `config.toml`. On other platforms (no keyring
backend yet) it falls back to `$GITHUB_TOKEN`, `gh auth token`, or a `0600`
file, and tells you which.

## Usage

```bash
lcpush             # the full interactive session
lcpush --refresh   # force a re-fetch of the problem list
```

That's the whole surface. `lcpush` is deliberately interactive-only: one
command, three prompts, done. The problem list is cached locally, so the
picker opens instantly - when the cache goes stale it is refreshed in the
background while you type, never while you wait. Only the very first run
(no cache yet) fetches in the foreground.

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
| `cli.cpp` | CLI11 flags, `config` subcommands, top-level error handling |
| `session.cpp` | The run flow, question to push |
| `onboarding.cpp` | First-run setup, token resolution |
| `config.cpp` `paths.cpp` `tokens.cpp` `keyring_*.cpp` | Persistence, XDG paths, keyring |
| `problems.cpp` `search.cpp` `picker.cpp` | Problem set, offline fuzzy match, picker UI |
| `clipboard.cpp` `editor.cpp` | Solution sources |
| `detect.cpp` `plausibility.cpp` `solution.cpp` | Language detection, scoring, validation |
| `render.cpp` `github.cpp` `ui.cpp` | Paths, commit messages, API, terminal output |
| `term/` `http/` `util/` | Raw mode + key decoding, libcurl transport, helpers |

## Development

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build           # 200+ tests
```

The prompt widgets are tested for real, driven through scripted key bytes
rather than mocked out - so "typing `two su` filters to Two Sum" and
"Ctrl-U clears the pre-filled commit message" are assertions, not hopes.
`tests/data/parity.json` holds vectors generated from the original Python
implementation, and a replay suite pins the C++ fuzzy-search ranking,
language-detection scores, and clipboard plausibility to them exactly.

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
