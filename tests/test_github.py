from __future__ import annotations

import base64
import json
import time

import httpx
import pytest

from lcpush.errors import GitHubError, TokenError
from lcpush.github import GitHubClient

TOKEN = "ghp_secret_token_value"


def _github(handler):
    return GitHubClient(TOKEN, client=httpx.Client(transport=httpx.MockTransport(handler)))


def test_get_repo_returns_default_branch():
    def handler(request):
        assert request.headers["authorization"] == f"Bearer {TOKEN}"
        assert request.headers["x-github-api-version"] == "2022-11-28"
        assert request.headers["accept"] == "application/vnd.github+json"
        return httpx.Response(
            200,
            json={
                "full_name": "user/solutions",
                "default_branch": "trunk",
                "permissions": {"push": True},
            },
        )

    with _github(handler) as github:
        info = github.get_repo("user", "solutions")
    assert info.default_branch == "trunk"
    assert info.can_push


def test_repo_404_names_the_repo():
    def handler(_request):
        return httpx.Response(404, json={"message": "Not Found"})

    with _github(handler) as github, pytest.raises(GitHubError) as excinfo:
        github.get_repo("user", "missing")
    assert excinfo.value.message == "Repo not found or token lacks access: user/missing"


def test_401_points_at_reset_token():
    def handler(_request):
        return httpx.Response(401, json={"message": "Bad credentials"})

    with _github(handler) as github, pytest.raises(TokenError) as excinfo:
        github.get_repo("user", "solutions")
    assert "lcpush config reset-token" in excinfo.value.message


def test_403_names_the_required_scope():
    def handler(_request):
        return httpx.Response(403, json={"message": "Resource not accessible"})

    with _github(handler) as github, pytest.raises(TokenError) as excinfo:
        github.get_repo("user", "solutions")
    assert "contents: read & write" in excinfo.value.message


def test_rate_limit_reports_reset_time():
    reset = int(time.time()) + 600

    def handler(_request):
        return httpx.Response(
            403,
            headers={"x-ratelimit-remaining": "0", "x-ratelimit-reset": str(reset)},
            json={"message": "rate limited"},
        )

    with _github(handler) as github, pytest.raises(GitHubError) as excinfo:
        github.get_repo("user", "solutions")
    assert "rate limit" in excinfo.value.message
    assert "Resets at" in excinfo.value.message


def test_get_file_sha_none_when_absent():
    def handler(_request):
        return httpx.Response(404, json={"message": "Not Found"})

    with _github(handler) as github:
        assert github.get_file_sha("user", "solutions", "0001-two-sum.py", "main") is None


def test_get_file_sha_present():
    def handler(request):
        assert request.url.params["ref"] == "main"
        return httpx.Response(200, json={"sha": "abc123"})

    with _github(handler) as github:
        assert github.get_file_sha("user", "solutions", "0001-two-sum.py", "main") == "abc123"


def test_put_new_file_sends_base64_and_no_sha():
    captured = {}

    def handler(request):
        captured.update(json.loads(request.content))
        return httpx.Response(
            201,
            json={
                "content": {"html_url": "https://github.com/user/solutions/blob/main/x.py"},
                "commit": {"sha": "deadbeef"},
            },
        )

    with _github(handler) as github:
        result = github.put_file(
            "user",
            "solutions",
            "0001-two-sum.py",
            content="print(1)\n",
            message="Add 1. Two Sum (Python3)",
            branch="main",
        )

    assert base64.b64decode(captured["content"]).decode() == "print(1)\n"
    assert captured["branch"] == "main"
    assert "sha" not in captured
    assert result.updated is False
    assert result.html_url.endswith("x.py")


def test_put_existing_file_includes_sha_and_author():
    captured = {}

    def handler(request):
        captured.update(json.loads(request.content))
        return httpx.Response(200, json={"content": {"html_url": "u"}, "commit": {"sha": "s"}})

    with _github(handler) as github:
        result = github.put_file(
            "user",
            "solutions",
            "0001-two-sum.py",
            content="x\n",
            message="Update",
            branch="main",
            sha="old",
            author_name="Larry",
            author_email="larry@example.com",
        )

    assert captured["sha"] == "old"
    assert captured["author"] == {"name": "Larry", "email": "larry@example.com"}
    assert result.updated is True


def test_conflict_refetches_sha_once_then_succeeds():
    calls = []

    def handler(request):
        calls.append((request.method, str(request.url)))
        if request.method == "PUT":
            if len([c for c in calls if c[0] == "PUT"]) == 1:
                return httpx.Response(409, json={"message": "conflict"})
            assert json.loads(request.content)["sha"] == "fresh"
            return httpx.Response(200, json={"content": {"html_url": "u"}, "commit": {"sha": "s"}})
        return httpx.Response(200, json={"sha": "fresh"})

    with _github(handler) as github:
        result = github.put_file(
            "user", "solutions", "p.py", content="x\n", message="m", branch="main", sha="stale"
        )
    assert result.html_url == "u"
    assert [c[0] for c in calls] == ["PUT", "GET", "PUT"]


def test_conflict_twice_fails_clearly():
    def handler(request):
        if request.method == "PUT":
            return httpx.Response(409, json={"message": "still conflicting"})
        return httpx.Response(200, json={"sha": "fresh"})

    with _github(handler) as github, pytest.raises(GitHubError) as excinfo:
        github.put_file(
            "user", "solutions", "p.py", content="x\n", message="m", branch="main", sha="stale"
        )
    assert "409" in excinfo.value.message


def test_network_failure_is_a_single_line():
    def handler(_request):
        raise httpx.ConnectError("no route to host")

    with _github(handler) as github, pytest.raises(GitHubError) as excinfo:
        github.get_repo("user", "solutions")
    assert excinfo.value.message.startswith("Could not reach GitHub")


def test_errors_never_leak_the_token():
    def handler(_request):
        return httpx.Response(500, json={"message": "server exploded"})

    with _github(handler) as github, pytest.raises(GitHubError) as excinfo:
        github.get_repo("user", "solutions")
    assert TOKEN not in excinfo.value.message


def test_paths_are_url_encoded():
    seen = {}

    def handler(request):
        seen["path"] = request.url.raw_path.decode()
        return httpx.Response(404, json={"message": "Not Found"})

    with _github(handler) as github:
        github.get_file_sha("user", "solutions", "sub dir/0001-two-sum.py", "main")
    assert "sub%20dir/0001-two-sum.py" in seen["path"]
