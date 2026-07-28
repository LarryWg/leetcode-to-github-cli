from __future__ import annotations

from lcpush.detect import detect
from lcpush.solution import (
    MAX_BYTES,
    brackets_balanced,
    byte_size,
    entry_point_names,
    line_count,
    matches_slug,
    normalize,
    reject_reason,
    soft_warnings,
)
from tests.conftest import CPP_SOLUTION, PY_SOLUTION


def test_normalize_converts_crlf_and_trims_trailing_space():
    assert normalize("a  \r\nb\t\r\n") == "a\nb\n"


def test_normalize_ensures_exactly_one_trailing_newline():
    assert normalize("x\n\n\n\n") == "x\n"
    assert normalize("x") == "x\n"


def test_normalize_preserves_content_byte_for_byte():
    assert normalize(CPP_SOLUTION) == CPP_SOLUTION


def test_normalize_empty():
    assert normalize("   \n\n") == ""


def test_line_and_byte_counts():
    assert line_count(PY_SOLUTION) == 8
    assert byte_size("héllo") == 6


def test_reject_empty():
    assert reject_reason("   \n ") == "Content is empty."


def test_reject_oversized():
    reason = reject_reason("x" * (MAX_BYTES + 1))
    assert reason is not None and "1MB" in reason


def test_accepts_normal_solution():
    assert reject_reason(PY_SOLUTION) is None


def test_entry_point_names_python():
    assert "twoSum" in entry_point_names(PY_SOLUTION)


def test_entry_point_names_cpp():
    assert "twoSum" in entry_point_names(CPP_SOLUTION)


def test_entry_point_names_javascript():
    js = "var lengthOfLongestSubstring = function(s) { return 0; };"
    assert entry_point_names(js) == ("lengthOfLongestSubstring",)


def test_brackets_balanced():
    assert brackets_balanced(PY_SOLUTION)
    assert brackets_balanced(CPP_SOLUTION)


def test_brackets_unbalanced_on_truncation():
    half = CPP_SOLUTION[: len(CPP_SOLUTION) // 2]
    assert not brackets_balanced(half)


def test_brackets_ignore_literals_and_comments():
    assert brackets_balanced('x = "{{{"  # )))\n')
    assert brackets_balanced("/* ( */ int a = 1;\n")


def test_matches_slug_camel_and_snake():
    assert matches_slug("twoSum", "two-sum")
    assert matches_slug("two_sum", "two-sum")
    assert matches_slug("TwoSum", "two-sum")
    assert not matches_slug("lengthOfLongestSubstring", "two-sum")


def test_matches_slug_is_permissive_without_a_slug():
    assert matches_slug("whatever", "")


def test_warning_on_unknown_language():
    warnings = soft_warnings("just some words here", detection=detect("just some words"))
    assert any("Could not identify" in w for w in warnings)


def test_warning_on_wrong_question():
    code = "var lengthOfLongestSubstring = function(s) { return 0; };\n"
    warnings = soft_warnings(code, slug="two-sum", title="Two Sum")
    assert any("lengthOfLongestSubstring" in w and "Two Sum" in w for w in warnings)


def test_no_wrong_question_warning_when_names_line_up():
    warnings = soft_warnings(PY_SOLUTION, slug="two-sum", title="Two Sum")
    assert not any("Wrong question" in w for w in warnings)


def test_warning_on_conflict_markers_and_todo():
    warnings = soft_warnings("<<<<<<< HEAD\nTODO: finish\n")
    assert any("conflict markers or TODOs" in w for w in warnings)


def test_warning_on_truncated_paste():
    half = CPP_SOLUTION[: len(CPP_SOLUTION) // 2]
    warnings = soft_warnings(half, detection=detect(half), slug="two-sum")
    assert any("Brackets are unbalanced" in w for w in warnings)


def test_clean_solution_has_no_warnings():
    assert soft_warnings(PY_SOLUTION, detection=detect(PY_SOLUTION), slug="two-sum") == ()
