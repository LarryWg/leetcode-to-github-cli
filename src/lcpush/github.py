"""GitHub Contents API client (spec §8.4, §9).

Every non-2xx response is translated into a single-line LcpushError. The token
appears in exactly one place — the Authorization header — and never in an
error message.
"""

from __future__ import annotations

import base64
from dataclasses import dataclass
from datetime import UTC, datetime

import httpx

from .errors import GitHubError, TokenError

API_ROOT = "https://api.github.com"
HEADERS = {
    "Accept": "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
}
REQUIRED_SCOPE = (
    "Token needs the `repo` scope (classic) or `contents: read & write` (fine-grained)."
)


@dataclass(frozen=True)
class RepoInfo:
    full_name: str
    default_branch: str
    can_push: bool


@dataclass(frozen=True)
class PushResult:
    html_url: str
    commit_sha: str
    updated: bool


class GitHubClient:
    """Thin wrapper over the three endpoints lcpush needs."""

    def __init__(self, token: str, *, client: httpx.Client | None = None) -> None:
        self._token = token
        self._client = client or httpx.Client(timeout=20.0)
        self._owns_client = client is None

    def __enter__(self) -> GitHubClient:
        return self

    def __exit__(self, *exc_info) -> None:
        self.close()

    def close(self) -> None:
        if self._owns_client:
            self._client.close()

    # -- plumbing ---------------------------------------------------------

    def _headers(self) -> dict[str, str]:
        return {**HEADERS, "Authorization": f"Bearer {self._token}"}

    def _request(self, method: str, path: str, **kwargs) -> httpx.Response:
        try:
            return self._client.request(
                method, f"{API_ROOT}{path}", headers=self._headers(), **kwargs
            )
        except httpx.HTTPError as exc:
            raise GitHubError(f"Could not reach GitHub: {exc}") from exc

    @staticmethod
    def _rate_limited(response: httpx.Response) -> bool:
        return (
            response.status_code == 403
            and response.headers.get("x-ratelimit-remaining") == "0"
        )

    @staticmethod
    def _rate_limit_message(response: httpx.Response) -> str:
        reset = response.headers.get("x-ratelimit-reset")
        when = ""
        if reset and reset.isdigit():
            stamp = datetime.fromtimestamp(int(reset), UTC).astimezone()
            when = f" Resets at {stamp.strftime('%H:%M:%S %Z')}."
        return f"GitHub API rate limit exceeded.{when}"

    def _raise_for(self, response: httpx.Response, *, repo: str) -> None:
        status = response.status_code
        if status == 401:
            raise TokenError(
                "GitHub token invalid or expired. Run: lcpush config reset-token"
            )
        if self._rate_limited(response):
            raise GitHubError(self._rate_limit_message(response))
        if status == 403:
            raise TokenError(f"GitHub denied write access to {repo}. {REQUIRED_SCOPE}")
        if status == 404:
            raise GitHubError(f"Repo not found or token lacks access: {repo}")
        if status == 422:
            detail = _detail(response)
            raise GitHubError(f"GitHub rejected the request: {detail}")
        raise GitHubError(f"GitHub returned {status}: {_detail(response)}")

    # -- endpoints --------------------------------------------------------

    def get_repo(self, owner: str, name: str) -> RepoInfo:
        """Verify access and read the default branch (setup flow, §3)."""
        repo = f"{owner}/{name}"
        response = self._request("GET", f"/repos/{owner}/{name}")
        if response.status_code != 200:
            self._raise_for(response, repo=repo)
        data = response.json()
        permissions = data.get("permissions") or {}
        return RepoInfo(
            full_name=data.get("full_name", repo),
            default_branch=data.get("default_branch") or "main",
            can_push=bool(permissions.get("push", True)),
        )

    def get_file_sha(
        self, owner: str, name: str, path: str, branch: str
    ) -> str | None:
        """The blob sha when the file already exists, else None (404)."""
        repo = f"{owner}/{name}"
        response = self._request(
            "GET",
            f"/repos/{owner}/{name}/contents/{_encode_path(path)}",
            params={"ref": branch},
        )
        if response.status_code == 404:
            return None
        if response.status_code != 200:
            self._raise_for(response, repo=repo)
        data = response.json()
        if isinstance(data, list):
            raise GitHubError(f"{path} is a directory in {repo}, not a file")
        return data.get("sha")

    def put_file(
        self,
        owner: str,
        name: str,
        path: str,
        *,
        content: str,
        message: str,
        branch: str,
        sha: str | None = None,
        author_name: str = "",
        author_email: str = "",
    ) -> PushResult:
        """Create or update a file, retrying once on a 409 sha conflict."""
        repo = f"{owner}/{name}"
        body: dict[str, object] = {
            "message": message,
            "content": base64.b64encode(content.encode("utf-8")).decode("ascii"),
            "branch": branch,
        }
        if sha:
            body["sha"] = sha
        if author_name and author_email:
            author = {"name": author_name, "email": author_email}
            body["author"] = author
            body["committer"] = author

        endpoint = f"/repos/{owner}/{name}/contents/{_encode_path(path)}"
        response = self._request("PUT", endpoint, json=body)

        if response.status_code == 409:
            # Someone else moved the file under us: re-fetch the sha once.
            fresh = self.get_file_sha(owner, name, path, branch)
            if fresh:
                body["sha"] = fresh
            else:
                body.pop("sha", None)
            response = self._request("PUT", endpoint, json=body)

        if response.status_code not in (200, 201):
            self._raise_for(response, repo=repo)

        data = response.json()
        content_block = data.get("content") or {}
        commit_block = data.get("commit") or {}
        return PushResult(
            html_url=content_block.get("html_url", ""),
            commit_sha=commit_block.get("sha", ""),
            updated=response.status_code == 200,
        )


def _encode_path(path: str) -> str:
    from urllib.parse import quote

    return quote(path.lstrip("/"), safe="/")


def _detail(response: httpx.Response) -> str:
    try:
        payload = response.json()
    except ValueError:
        return response.text.strip()[:200] or response.reason_phrase
    if isinstance(payload, dict):
        message = str(payload.get("message", "")).strip()
        errors = payload.get("errors")
        if isinstance(errors, list) and errors:
            first = errors[0]
            if isinstance(first, dict) and first.get("message"):
                return f"{message} ({first['message']})" if message else str(first["message"])
        return message or response.reason_phrase
    return response.reason_phrase
