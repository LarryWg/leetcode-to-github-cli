# `lcpush` — Spec

A CLI tool that pushes a LeetCode solution to a GitHub repo in one interactive session, from any working directory.

---

## 1. Goals

- Zero-friction: `lcpush`, three prompts, done.
- Runs from **any** directory. No local clone, no working tree, no `cd`.
- Repo target is configured once and persisted.
- Question selection is a fuzzy, type-to-filter picker over the real LeetCode problem set.
- Language is auto-detected from the pasted code and confirmed with one keypress.

## 2. Non-goals (v1)

- No LeetCode account auth, no submission scraping, no "fetch my accepted solutions."
- No multi-file solutions, no test harnesses, no README index generation.
- No support for hosts other than GitHub.
- No editing/deleting of previously pushed solutions beyond overwrite-on-confirm.

---

## 3. User flow

```
$ lcpush

  (first run only)
  ? GitHub repo to push to:  user/leetcode-solutions
  ? GitHub token:            ****   [auto-detected from `gh auth token` if available]
  ✓ Verified write access to user/leetcode-solutions
  ✓ Saved config to ~/.config/lcpush/config.toml

  ? Question:  two su
      1.  Two Sum                                    [Easy]
      167. Two Sum II - Input Array Is Sorted        [Medium]
      653. Two Sum IV - Input is a BST               [Easy]
      1099. Two Sum Less Than K                      [Easy] 🔒
    ↑/↓ to move, Enter to select

  ✓ 1. Two Sum

  ? Solution source:  [Clipboard]  Editor  Stdin
  ✓ Read 24 lines from clipboard

  ? Language:  Python3  (detected)
    ↑/↓ to change, Enter to accept

  ┌ Ready to push
  │ File     0001-two-sum.py  (24 lines)
  │ Repo     user/leetcode-solutions  (main)
  │ Message  Add 1. Two Sum (Python3)
  └ ? [Enter] push   [m] edit message   [n] cancel

  → Pushing 0001-two-sum.py to user/leetcode-solutions (main)
  ✓ https://github.com/user/leetcode-solutions/blob/main/0001-two-sum.py
```

Pressing `m` opens the message pre-filled and cursor-at-end for inline editing, then returns to this panel:

```
  ? Commit message:  Add 1. Two Sum (Python3)█
    Enter to confirm, Ctrl-U to clear
```

Subsequent runs skip step 1 entirely.

---

## 4. Configuration

Location: `$XDG_CONFIG_HOME/lcpush/config.toml`, defaulting to `~/.config/lcpush/config.toml`.
Cache: `$XDG_CACHE_HOME/lcpush/`, defaulting to `~/.cache/lcpush/`.

```toml
[repo]
owner  = "user"
name   = "leetcode-solutions"
branch = "main"          # resolved from the repo's default branch at setup
path   = ""              # optional subdirectory prefix, e.g. "solutions/"

[commit]
message_template = "Add {id}. {title} ({language})"
update_template  = "Update {id}. {title} ({language})"
prompt = "confirm"       # "confirm" = show in the ready-to-push panel, editable via [m]
                         # "always"  = always open the message field for editing
                         # "never"   = use the rendered template, no panel edit option
author_name  = ""        # empty = use the token's GitHub identity
author_email = ""

[cache]
problems_ttl_days = 7
```

Config file must be written with mode `0600`.

### Token storage

Resolution order, first hit wins:

1. `$LCPUSH_GITHUB_TOKEN` env var
2. `$GITHUB_TOKEN` env var
3. OS keyring (`keyring` library — macOS Keychain / libsecret / Windows Credential Manager)
4. `gh auth token` if the GitHub CLI is installed and authenticated
5. Interactive prompt (hidden input) → store in keyring

**Never write the token to `config.toml`.** If the keyring is unavailable, warn the user and fall back to `~/.config/lcpush/token` with mode `0600`.

Required token scope: `repo` (classic) or `contents: read & write` (fine-grained).

### Config commands

```
lcpush config show                       # prints config, token redacted
lcpush config set repo <owner/name>      # re-verifies write access
lcpush config set branch <name>
lcpush config set path <prefix>
lcpush config set commit.message_template "<template>"
lcpush config set commit.prompt <confirm|always|never>
lcpush config reset-token
lcpush --repo owner/other-repo           # one-shot override, not persisted
lcpush -m "Solve Two Sum with a hash map"   # one-shot commit message, skips the edit prompt
```

---

## 5. Question search

### 5.1 Problem list cache

On first run (or when the cache is stale/missing), fetch the entire public problem set and write it to `~/.cache/lcpush/problems.json`.

Endpoint: `POST https://leetcode.com/graphql`

Headers:
```
Content-Type: application/json
Referer: https://leetcode.com/problemset/all/
User-Agent: <a normal browser UA string>
```

Query:
```graphql
query problemsetQuestionList($categorySlug: String, $limit: Int, $skip: Int, $filters: QuestionListFilterInput) {
  problemsetQuestionList: questionList(
    categorySlug: $categorySlug
    limit: $limit
    skip: $skip
    filters: $filters
  ) {
    total: totalNum
    questions: data {
      frontendQuestionId: questionFrontendId
      title
      titleSlug
      difficulty
      paidOnly: isPaidOnly
    }
  }
}
```

Variables: `{"categorySlug": "all-code-essentials", "skip": <n>, "limit": 500, "filters": {}}`

Paginate on `skip` until `len(collected) >= total`. If `limit: 500` is rejected, degrade to 100. Sleep ~200ms between pages. If the field name `all-code-essentials` fails, retry with `categorySlug: ""`.

Cached record shape:
```json
{
  "fetched_at": "2026-07-27T10:00:00Z",
  "questions": [
    {"id": "1", "title": "Two Sum", "slug": "two-sum", "difficulty": "Easy", "paid": false}
  ]
}
```

Refresh when older than `problems_ttl_days`, or on `lcpush --refresh`. A stale cache is always preferred over a failed run — if the network fetch fails and a cache exists, use it and print a dim warning.

### 5.2 Picker behaviour

- Fuzzy match against `"{id}. {title}"` and `slug`. Use RapidFuzz (`WRatio` or `partial_ratio`).
- If the input is all digits, exact-prefix match on `id` ranks first.
- Show top 10 matches, `[Difficulty]` right-aligned, 🔒 suffix for `paid: true`.
- No network calls during typing.
- Escape hatch: `lcpush --slug two-sum` and `lcpush --id 1` skip the picker entirely.

---

## 6. Solution input

### 6.1 Sources

1. **Clipboard** — `pbpaste` (macOS), `wl-paste` (Wayland), `xclip -selection clipboard -o` / `xsel -b` (X11), `powershell Get-Clipboard` (Windows). Omit this option entirely if no clipboard tool exists or the clipboard is empty/whitespace-only.
2. **Editor** — write a temp file with a commented header (`# Paste your solution below. Save and close to continue.`), open `$VISUAL` → `$EDITOR` → `vi`, strip the header, read the result. Abort if the file comes back empty or unchanged.
3. **Stdin** — read until EOF (Ctrl-D) or a lone `EOF` sentinel line.

Flags: `--no-clipboard` removes the option; `--editor` and `--stdin` jump straight to that source.

### 6.2 Clipboard plausibility scoring

Before the source menu is drawn, read the clipboard and score it. This determines **menu ordering only** — it never blocks a choice.

Positive signals:
- The language detector (§7) returns a winner above its confidence threshold.
- Content contains a LeetCode entry-point shape: `class Solution`, `impl Solution`, `object Solution`, `public class Solution`, `var \w+ = function(`, `func \w+(`, `def \w+(`.
- Two or more lines.
- Contains at least one of `{`, `:`, `(`, and leading indentation on some line.

Negative signals:
- Matches a bare URL, an email address, or a filesystem path and nothing else.
- Single line under 40 characters.
- Very high ratio of prose words to punctuation/symbols (heuristic for chat messages and articles).
- Over 200KB.

Outcome:
- **Plausible** → `Clipboard` is the preselected menu option.
- **Not plausible** → the option is rendered as `Clipboard (doesn't look like code)` and `Editor` is preselected instead. The clipboard remains selectable.

### 6.3 Preview and confirmation — required

Whatever the source, after reading and before anything else, print a preview and require confirmation. This is the primary guard against a wrong or stale clipboard, and it is **not skippable in interactive mode**.

```
  ┌ Clipboard — 24 lines, 612 bytes, detected Python3
  │ class Solution:
  │     def twoSum(self, nums: List[int], target: int) -> List[int]:
  │         seen = {}
  │         for i, n in enumerate(nums):
  │  … 16 lines hidden …
  │         return []
  └ ? Use this? [Y/n]
```

Rules:
- Show the **first 5 and last 3 lines**. Showing only the head hides truncated pastes, which are more common than wrong-clipboard errors.
- Show line count and byte count. A 2-line count when you expected 25 is the fastest possible truncation signal.
- Show the detected language in the header — a detection of "unknown" is itself strong evidence the content isn't a solution.
- `n` returns to the source menu rather than exiting, so a bad clipboard costs one keypress, not a restarted session.

### 6.4 Soft warnings

Printed above the confirm prompt; none of them block.

| Check | Warning |
|---|---|
| No language detected above threshold | `⚠ Could not identify a programming language.` |
| Entry-point function name doesn't fuzzy-match the selected question's slug (e.g. slug `two-sum` → expect `twoSum`/`two_sum`) | `⚠ This defines lengthOfLongestSubstring, but you selected "Two Sum". Wrong question?` |
| Content contains `<<<<<<<`, `>>>>>>>`, or `TODO` | `⚠ Content contains conflict markers or TODOs.` |
| Content ends mid-token (unbalanced brackets) | `⚠ Brackets are unbalanced — the paste may be truncated.` |

The slug↔function-name mapping is not reliable enough to ever block on: LeetCode names are inconsistent, and some languages use snake_case. Warn and move on.

### 6.5 Hard rejections

Only two, both re-prompting rather than exiting:
- Empty or whitespace-only content.
- Over 1MB.

### 6.6 Post-processing

- Strip trailing whitespace per line, normalize CRLF → LF, ensure exactly one trailing newline.
- Do not reformat, lint, or otherwise modify the code.

### 6.7 Non-interactive mode

`cat sol.py | lcpush --slug two-sum --lang python3 --force` reads stdin and skips the preview and all prompts. Soft warnings still print to stderr. `--force` is required for any non-interactive run precisely because the preview guard is absent.

---

## 7. Language detection

Score each candidate language by matching regex signals; the highest score wins. Present it preselected in a list of all supported languages sorted by score.

| Language | Signals |
|---|---|
| C++ | `#include <`, `std::`, `vector<`, `public:` inside `class Solution`, `nullptr` |
| C | `#include <stdlib.h>`, `int*`, `malloc(`, **and** no `class`/`std::` |
| Python3 | `class Solution:`, `def ` + `self`, `List[int]`, `->` return annotation |
| Java | `import java.`, `public class Solution`, `int[] `, `new ArrayList<>` |
| C# | `using System`, `public class Solution`, `IList<`, `int[] ` with PascalCase methods |
| JavaScript | `/** @param {number[]}`, `var `/`const ` + `function(`, `module.exports` |
| TypeScript | `function f(nums: number[]): number[]`, `: string`/`: number` param annotations |
| Go | `func `, `package `, `:=`, `[]int` |
| Rust | `impl Solution`, `pub fn`, `Vec<i32>`, `&self`, `->` with `i32` |
| Ruby | `def ` + `end`, `# @param {Integer[]}`, `nil` |
| Swift | `class Solution {` + `func ` + `-> [Int]`, `var ` with `let` |
| Kotlin | `fun `, `class Solution {`, `IntArray` |
| Scala | `object Solution`, `def ` + `: Array[Int]` |
| PHP | `<?php`, `function ` + `$` variables |

Known ambiguities to handle explicitly, since confirmation is the safety net:
- **C vs C++** — C only when no `class`, no `std::`, no `<iostream>`/`<vector>`.
- **Java vs C#** — `using System` or `IList<` forces C#.
- **JS vs TS** — any type annotation on a parameter or return forces TS.

Extension map:
```
cpp: .cpp    c: .c        python3: .py    java: .java   csharp: .cs
javascript: .js           typescript: .ts golang: .go   rust: .rs
ruby: .rb    swift: .swift kotlin: .kt    scala: .scala php: .php
```

Override: `--lang python3` skips detection and the prompt.

If no language scores above a minimum threshold, do not guess — show the full list unselected and require an explicit choice.

---

## 8. Commit message and push

### 8.1 Target path

Target path: `{config.repo.path}{id:0>4}-{slug}{ext}` → e.g. `0001-two-sum.py`, `0167-two-sum-ii-input-array-is-sorted.cpp`.

IDs are zero-padded to 4 digits. Non-numeric IDs (rare, contest-only problems) are used verbatim.

### 8.2 Commit message

Rendered from `commit.message_template` (or `commit.update_template` when overwriting an existing file) with these variables available:

| Variable | Example |
|---|---|
| `{id}` | `1` |
| `{padded_id}` | `0001` |
| `{title}` | `Two Sum` |
| `{slug}` | `two-sum` |
| `{language}` | `Python3` |
| `{ext}` | `.py` |
| `{difficulty}` | `Easy` |
| `{filename}` | `0001-two-sum.py` |
| `{lines}` | `24` |

Precedence, first wins:
1. `-m` / `--message` flag — used verbatim, no prompt.
2. Interactive edit via `[m]` in the ready-to-push panel.
3. The rendered template.

The template is never presented as a blank field. When `commit.prompt = "always"` or the user presses `[m]`, the input is **pre-filled with the rendered template and the cursor placed at the end**, so accepting is a single keypress and editing is additive.

Rules:
- Trim trailing whitespace; reject empty (re-prompt, do not fall through to a default silently).
- Multi-line messages are supported. The first line is the subject; if the user needs a body, `[M]` (shift-m) opens `$EDITOR` with the rendered message pre-loaded, git-commit style.
- A message longer than 72 characters on the first line prints a soft warning but is accepted.
- If a template variable is misspelled in config, fall back to the built-in default and warn once — never crash on a bad template.

### 8.3 Ready-to-push panel

Shown in interactive mode immediately before the API call. Displays filename, line count, target repo and branch, whether this is an add or an overwrite, and the commit message.

Keys: `Enter` push · `m` edit message inline · `M` edit message in `$EDITOR` · `n` cancel and exit without pushing.

When overwriting, the panel header reads `Ready to push (overwrites existing file)` — this replaces the separate overwrite prompt in §8.4 rather than adding a second confirmation.

### 8.4 API calls

1. `GET /repos/{owner}/{repo}/contents/{path}?ref={branch}` — returns 404 if the file is new, or 200 with a `sha` if it already exists. This runs **before** the panel is drawn so the panel can show add-vs-overwrite and pick the right template.
2. `PUT /repos/{owner}/{repo}/contents/{path}` with body:
   ```json
   {
     "message": "Add 1. Two Sum (Python3)",
     "content": "<base64 of the file>",
     "branch": "main",
     "sha": "<only when overwriting>"
   }
   ```

Headers: `Authorization: Bearer <token>`, `Accept: application/vnd.github+json`, `X-GitHub-Api-Version: 2022-11-28`.

`--force` skips the panel entirely; `--no-clobber` aborts instead of overwriting.

On success, print the `content.html_url` from the response.

---

## 9. Error handling

Every one of these must produce a single clear line and a non-zero exit code — no stack traces.

| Condition | Behaviour |
|---|---|
| No config on first run | Enter setup flow |
| Repo not found / 404 on verify | `Repo not found or token lacks access: owner/name` |
| Token missing or 401 | `GitHub token invalid or expired. Run: lcpush config reset-token` |
| Token lacks write scope (403) | Name the required scope explicitly |
| LeetCode fetch fails, cache exists | Warn, proceed with stale cache |
| LeetCode fetch fails, no cache | `Could not reach LeetCode and no cached problem list. Retry, or use --slug.` |
| Clipboard empty / tool missing | Silently drop the clipboard option from the source menu |
| Editor exits with empty buffer | `No solution provided. Aborted.` |
| 409 conflict on PUT | Re-fetch the sha once and retry, then fail |
| GitHub rate limit (403 + `x-ratelimit-remaining: 0`) | Report the reset time from the header |
| Ctrl-C at any prompt | Exit 130 cleanly, no partial writes |

---

## 10. Suggested stack

Python 3.11+, installed via `pipx install lcpush`.

- `typer` — CLI framework and flag parsing
- `questionary` or `prompt_toolkit` — the fuzzy picker and select prompts
- `rapidfuzz` — fuzzy matching
- `httpx` — HTTP client for both LeetCode and GitHub
- `keyring` — token storage
- `platformdirs` — XDG-correct config and cache paths

Entry point: `lcpush = "lcpush.cli:app"`.

Nothing in the tool should assume a git binary, a clone, or a working directory.

---

## 11. Acceptance criteria

1. `lcpush` from `/tmp`, `~`, or any repo directory behaves identically.
2. First run persists the repo; the second run never asks for it again.
3. Typing `two su` in the picker filters without any network request.
4. A pasted C++ solution produces `NNNN-slug.cpp` with the correct content, byte-for-byte, plus a trailing newline.
5. Pushing the same question twice prompts before overwriting and succeeds when confirmed.
6. Running with no network but a warm cache still works up to the push step, then fails with a clear message.
7. The token never appears in `config.toml`, `lcpush config show`, logs, or error output.
8. `cat sol.py | lcpush --slug two-sum --lang python3 --force` completes with zero prompts.
9. With a URL on the clipboard, `Clipboard` is not preselected and is labeled as not looking like code.
10. Declining the preview with `n` returns to the source menu; nothing is pushed and the session continues.
11. A clipboard containing the first half of a solution shows an unbalanced-bracket warning and a tail preview that makes the truncation visible.
12. The ready-to-push panel shows a non-empty commit message by default; pressing Enter uses it unmodified.
13. Pressing `m` presents that same message pre-filled and editable, not a blank field.
14. `-m "custom"` uses the message verbatim and never shows the message edit prompt.
15. Overwriting an existing file renders the update template and is confirmed by the panel alone — the user is never asked twice.

---

## 12. Deferred to v2 (design so as not to preclude)

- Auto-generated `README.md` index table.
- Directory grouping by difficulty or topic tag (the `path` config field is the seam for this).
- Pushing multiple languages for one question in one invocation.
- `lcpush log` — a local history of what has been pushed.
- Solution notes appended as a header comment (time/space complexity, approach).
