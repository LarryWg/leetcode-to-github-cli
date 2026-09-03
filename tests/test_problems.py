from __future__ import annotations

import json
from datetime import UTC, datetime, timedelta

import httpx
import pytest

from lcpush import problems
from lcpush.errors import LeetCodeError
from lcpush.paths import problems_cache_file
from lcpush.problems import (
    Question,
    fetch_all,
    get_questions,
    is_stale,
    load_cache,
    save_cache,
)

NOW = datetime(2026, 7, 27, 10, 0, tzinfo=UTC)


def _page(total, start, count):
    return {
        "data": {
            "problemsetQuestionList": {
                "total": total,
                "questions": [
                    {
                        "frontendQuestionId": str(index),
                        "title": f"Problem {index}",
                        "titleSlug": f"problem-{index}",
                        "difficulty": "Easy",
                        "paidOnly": False,
                    }
                    for index in range(start, start + count)
                ],
            }
        }
    }


def _client(handler):
    return httpx.Client(transport=httpx.MockTransport(handler))


def test_fetch_paginates_until_total():
    calls = []

    def handler(request: httpx.Request) -> httpx.Response:
        body = json.loads(request.content)
        skip = body["variables"]["skip"]
        calls.append(skip)
        start = skip + 1
        count = min(500, 1200 - skip)
        return httpx.Response(200, json=_page(1200, start, count))

    with _client(handler) as client:
        questions = fetch_all(client, sleep=lambda _seconds: None)

    assert len(questions) == 1200
    assert calls == [0, 500, 1000]
    assert questions[0].id == "1"


def test_fetch_advances_by_actual_page_size_when_the_server_caps_it():
    """LeetCode caps pages at 100 however large `limit` is; stride must follow."""
    skips = []

    def handler(request: httpx.Request) -> httpx.Response:
        skip = json.loads(request.content)["variables"]["skip"]
        skips.append(skip)
        count = max(0, min(100, 250 - skip))  # server-side cap, ignoring limit
        return httpx.Response(200, json=_page(250, skip + 1, count))

    with _client(handler) as client:
        questions = fetch_all(client, sleep=lambda _seconds: None)

    assert skips == [0, 100, 200]
    assert len(questions) == 250
    assert [q.id for q in questions] == [str(n) for n in range(1, 251)]


def test_fetch_degrades_page_size_then_category():
    attempts = []

    def handler(request: httpx.Request) -> httpx.Response:
        body = json.loads(request.content)
        variables = body["variables"]
        attempts.append((variables["categorySlug"], variables["limit"]))
        if variables["limit"] == 500:
            return httpx.Response(400, json={"errors": [{"message": "limit too high"}]})
        if variables["categorySlug"] == "all-code-essentials":
            return httpx.Response(200, json={"errors": [{"message": "bad category"}]})
        return httpx.Response(200, json=_page(2, 1, 2))

    with _client(handler) as client:
        questions = fetch_all(client, sleep=lambda _seconds: None)

    assert len(questions) == 2
    assert ("", 100) in attempts


def test_fetch_raises_when_everything_fails():
    def handler(_request):
        return httpx.Response(500, text="boom")

    with _client(handler) as client:
        with pytest.raises(LeetCodeError):
            fetch_all(client, sleep=lambda _seconds: None)


def test_cache_round_trip(questions):
    path = save_cache(questions, now=NOW)
    stamp, loaded = load_cache(path)
    assert stamp == NOW
    assert loaded == questions
    record = json.loads(path.read_text())
    assert record["fetched_at"] == "2026-07-27T10:00:00Z"


def test_load_cache_missing_and_corrupt():
    assert load_cache() == (None, ())
    path = problems_cache_file()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("{not json", encoding="utf-8")
    assert load_cache(path) == (None, ())


def test_is_stale():
    assert is_stale(None, 7)
    assert not is_stale(NOW, 7, now=NOW + timedelta(days=6))
    assert is_stale(NOW, 7, now=NOW + timedelta(days=8))
    assert is_stale(NOW, 0, now=NOW)


def test_get_questions_uses_fresh_cache(questions):
    save_cache(questions, now=NOW)

    def factory():  # pragma: no cover - must never be called
        raise AssertionError("network was used despite a fresh cache")

    assert get_questions(ttl_days=7, client_factory=factory, now=NOW) == questions


def test_get_questions_serves_stale_cache_and_refreshes_in_background(
    questions, monkeypatch
):
    """A stale cache must never block startup on the network."""
    save_cache(questions, now=NOW - timedelta(days=30))
    spawned = []
    monkeypatch.setattr(
        problems, "_spawn_refresh", lambda target, factory: spawned.append(target)
    )

    def factory():  # pragma: no cover - must never be called in the foreground
        raise AssertionError("stale cache blocked on a foreground fetch")

    result = get_questions(ttl_days=7, client_factory=factory, now=NOW)
    assert result == questions
    assert spawned == [problems_cache_file()]


def test_background_refresh_rewrites_the_cache(questions):
    save_cache(questions, now=NOW - timedelta(days=30))

    def factory():
        def handler(_request):
            return httpx.Response(200, json=_page(2, 1, 2))

        return _client(handler)

    thread = problems._spawn_refresh(problems_cache_file(), factory)
    thread.join(timeout=5)
    _stamp, refreshed = load_cache()
    assert len(refreshed) == 2


def test_background_refresh_failure_keeps_the_stale_cache(questions):
    save_cache(questions, now=NOW - timedelta(days=30))

    def factory():
        def handler(_request):
            raise httpx.ConnectError("offline")

        return _client(handler)

    thread = problems._spawn_refresh(problems_cache_file(), factory)
    thread.join(timeout=5)
    _stamp, kept = load_cache()
    assert kept == questions


def test_get_questions_without_cache_or_network():
    def factory():
        def handler(_request):
            raise httpx.ConnectError("offline")

        return _client(handler)

    with pytest.raises(LeetCodeError) as excinfo:
        get_questions(ttl_days=7, client_factory=factory, now=NOW)
    assert "no cached problem list" in excinfo.value.message


def test_get_questions_first_run_announces_the_fetch():
    notes = []

    def factory():
        def handler(_request):
            return httpx.Response(200, json=_page(2, 1, 2))

        return _client(handler)

    get_questions(ttl_days=7, client_factory=factory, info=notes.append, now=NOW)
    assert notes and "first run" in notes[0]


def test_get_questions_explicit_refresh_falls_back_to_cache_with_a_warning(questions):
    save_cache(questions, now=NOW)
    warnings = []

    def factory():
        def handler(_request):
            raise httpx.ConnectError("offline")

        return _client(handler)

    result = get_questions(
        ttl_days=7, refresh=True, client_factory=factory, warn=warnings.append, now=NOW
    )
    assert result == questions
    assert warnings and "using cache" in warnings[0]


def test_get_questions_refresh_writes_cache():
    def factory():
        def handler(_request):
            return httpx.Response(200, json=_page(2, 1, 2))

        return _client(handler)

    result = get_questions(ttl_days=7, refresh=True, client_factory=factory, now=NOW)
    assert len(result) == 2
    assert problems_cache_file().exists()


def test_padded_id_handles_non_numeric():
    assert Question("42", "T", "t", "Easy").padded_id == "0042"
    assert Question("LCP 01", "T", "t", "Easy").padded_id == "LCP 01"
