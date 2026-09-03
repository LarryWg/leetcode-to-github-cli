"""LeetCode problem-set fetch and cache (spec §5.1).

A cache always beats a blocking fetch: a stale cache is served immediately
while a background thread refreshes it for the next run. Only the very first
run (no cache at all) or an explicit --refresh waits on the network.
"""

from __future__ import annotations

import json
import os
import threading
import time
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from pathlib import Path

import httpx

from .errors import LeetCodeError
from .paths import problems_cache_file

GRAPHQL_URL = "https://leetcode.com/graphql"
USER_AGENT = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
)
HEADERS = {
    "Content-Type": "application/json",
    "Referer": "https://leetcode.com/problemset/all/",
    "User-Agent": USER_AGENT,
}

QUERY = """
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
"""

CATEGORY_SLUGS = ("all-code-essentials", "")
PAGE_SIZES = (500, 100)
PAGE_DELAY_SECONDS = 0.2


@dataclass(frozen=True)
class Question:
    id: str
    title: str
    slug: str
    difficulty: str
    paid: bool = False

    @property
    def display(self) -> str:
        return f"{self.id}. {self.title}"

    @property
    def padded_id(self) -> str:
        """Zero-pad numeric ids to 4 digits; use rare non-numeric ids verbatim."""
        return self.id.zfill(4) if self.id.isdigit() else self.id


def _to_question(raw: dict) -> Question:
    return Question(
        id=str(raw.get("frontendQuestionId", "")).strip(),
        title=str(raw.get("title", "")).strip(),
        slug=str(raw.get("titleSlug", "")).strip(),
        difficulty=str(raw.get("difficulty", "") or "Unknown"),
        paid=bool(raw.get("paidOnly", False)),
    )


def _fetch_page(
    client: httpx.Client, *, category: str, limit: int, skip: int
) -> tuple[int, list[Question]]:
    response = client.post(
        GRAPHQL_URL,
        headers=HEADERS,
        json={
            "query": QUERY,
            "variables": {
                "categorySlug": category,
                "skip": skip,
                "limit": limit,
                "filters": {},
            },
        },
    )
    response.raise_for_status()
    payload = response.json()
    if payload.get("errors"):
        messages = "; ".join(
            str(err.get("message", err)) for err in payload["errors"]
        )
        raise LeetCodeError(f"LeetCode rejected the query: {messages}")
    block = (payload.get("data") or {}).get("problemsetQuestionList")
    if not block:
        raise LeetCodeError("LeetCode returned no problem list")
    total = int(block.get("total") or 0)
    questions = [_to_question(item) for item in block.get("questions") or []]
    return total, questions


def fetch_all(client: httpx.Client, *, sleep=time.sleep) -> tuple[Question, ...]:
    """Fetch the whole public problem set, paginating on `skip`.

    Degrades `limit` 500 -> 100 and `categorySlug` -> "" if the first attempt
    is rejected, per spec §5.1.
    """
    last_error: Exception | None = None
    for category in CATEGORY_SLUGS:
        for limit in PAGE_SIZES:
            try:
                collected: list[Question] = []
                skip = 0
                total, page = _fetch_page(
                    client, category=category, limit=limit, skip=skip
                )
                collected.extend(page)
                # Advance by what the server actually returned, not by what we
                # asked for: LeetCode silently caps pages at 100 however large
                # `limit` is, and striding by `limit` would skip most problems.
                while page and len(collected) < total:
                    skip += len(page)
                    sleep(PAGE_DELAY_SECONDS)
                    _, page = _fetch_page(
                        client, category=category, limit=limit, skip=skip
                    )
                    collected.extend(page)
                if collected:
                    return tuple(collected)
                last_error = LeetCodeError("LeetCode returned an empty problem list")
            except (httpx.HTTPError, LeetCodeError, ValueError, KeyError) as exc:
                last_error = exc
    raise LeetCodeError(f"Could not fetch the LeetCode problem list: {last_error}")


def save_cache(
    questions: tuple[Question, ...],
    *,
    path: Path | None = None,
    now: datetime | None = None,
) -> Path:
    target = path or problems_cache_file()
    target.parent.mkdir(parents=True, exist_ok=True)
    stamp = (now or datetime.now(UTC)).astimezone(UTC).replace(microsecond=0)
    record = {
        "fetched_at": stamp.isoformat().replace("+00:00", "Z"),
        "questions": [
            {
                "id": q.id,
                "title": q.title,
                "slug": q.slug,
                "difficulty": q.difficulty,
                "paid": q.paid,
            }
            for q in questions
        ],
    }
    # Atomic write: the background refresh must never leave a torn cache
    scratch = target.with_name(target.name + ".tmp")
    scratch.write_text(json.dumps(record), encoding="utf-8")
    os.replace(scratch, target)
    return target


def load_cache(
    path: Path | None = None,
) -> tuple[datetime | None, tuple[Question, ...]]:
    """Return (fetched_at, questions); ((None, ()) when there is no usable cache."""
    target = path or problems_cache_file()
    if not target.exists():
        return None, ()
    try:
        record = json.loads(target.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        return None, ()
    stamp: datetime | None = None
    raw_stamp = record.get("fetched_at")
    if isinstance(raw_stamp, str):
        try:
            stamp = datetime.fromisoformat(raw_stamp.replace("Z", "+00:00"))
        except ValueError:
            stamp = None
    questions = tuple(
        Question(
            id=str(item.get("id", "")),
            title=str(item.get("title", "")),
            slug=str(item.get("slug", "")),
            difficulty=str(item.get("difficulty", "Unknown")),
            paid=bool(item.get("paid", False)),
        )
        for item in record.get("questions") or []
        if item.get("id") and item.get("slug")
    )
    return stamp, questions


def is_stale(
    fetched_at: datetime | None, ttl_days: int, *, now: datetime | None = None
) -> bool:
    if fetched_at is None:
        return True
    if ttl_days <= 0:
        return True
    current = now or datetime.now(UTC)
    if fetched_at.tzinfo is None:
        fetched_at = fetched_at.replace(tzinfo=UTC)
    return current - fetched_at > timedelta(days=ttl_days)


def _default_client() -> httpx.Client:
    return httpx.Client(timeout=20.0)


def _spawn_refresh(target: Path, client_factory) -> threading.Thread:
    """Refresh the cache on a daemon thread so startup never waits on it.

    Failures are silently dropped: the stale cache stays in place and the
    next run simply tries again.
    """

    def work() -> None:
        factory = client_factory or _default_client
        try:
            with factory() as client:
                fresh = fetch_all(client)
            save_cache(fresh, path=target)
        except (LeetCodeError, httpx.HTTPError, OSError):
            pass

    thread = threading.Thread(target=work, daemon=True, name="lcpush-cache-refresh")
    thread.start()
    return thread


def get_questions(
    *,
    ttl_days: int = 7,
    refresh: bool = False,
    path: Path | None = None,
    client_factory=None,
    warn=lambda message: None,
    info=lambda message: None,
    now: datetime | None = None,
) -> tuple[Question, ...]:
    """Cached problem set. A stale cache is served immediately and refreshed
    in the background; only a missing cache or `refresh` blocks on the fetch.
    """
    target = path or problems_cache_file()
    fetched_at, cached = load_cache(target)
    if cached and not refresh:
        if is_stale(fetched_at, ttl_days, now=now):
            _spawn_refresh(target, client_factory)
        return cached

    if not cached:
        info("Fetching the LeetCode problem list (first run only, ~30s)...")
    factory = client_factory or _default_client
    try:
        with factory() as client:
            fresh = fetch_all(client)
    except (LeetCodeError, httpx.HTTPError, OSError) as exc:
        if cached:
            warn(f"Could not refresh the LeetCode problem list ({exc}); using cache.")
            return cached
        raise LeetCodeError(
            "Could not reach LeetCode and no cached problem list. "
            "Check your connection and retry."
        ) from exc
    save_cache(fresh, path=target, now=now)
    return fresh
