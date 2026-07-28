from __future__ import annotations

from lcpush.plausibility import assess
from tests.conftest import CPP_SOLUTION, PY_SOLUTION


def test_solution_is_plausible():
    assert assess(PY_SOLUTION).plausible
    assert assess(CPP_SOLUTION).plausible


def test_empty_clipboard_is_not_plausible():
    result = assess("")
    assert not result.plausible
    assert result.reasons == ("clipboard is empty",)
    assert not assess(None).plausible


def test_bare_url_is_not_plausible():
    result = assess("https://leetcode.com/problems/two-sum/")
    assert not result.plausible
    assert any("bare URL" in reason for reason in result.reasons)


def test_email_and_path_are_not_plausible():
    assert not assess("someone@example.com").plausible
    assert not assess("/Users/larry/Downloads/notes.txt").plausible


def test_short_single_line_is_not_plausible():
    assert not assess("two sum").plausible


def test_prose_is_not_plausible():
    prose = (
        "Hey, I was reading about the two sum problem yesterday and it turns out "
        "the hash map approach is by far the most common way people solve it "
        "during interviews these days"
    )
    assert not assess(prose).plausible


def test_huge_content_is_penalized():
    big = PY_SOLUTION + "\n" + ("# padding\n" * 40000)
    result = assess(big)
    assert any("200KB" in reason for reason in result.reasons)
