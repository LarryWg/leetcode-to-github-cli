from __future__ import annotations

from lcpush.search import build_index, search


def test_empty_query_lists_by_id(questions):
    index = build_index(questions)
    results = search(index, "", limit=3)
    assert [q.id for q in results] == ["1", "2", "3"]


def test_prefix_typing_filters_offline(questions):
    index = build_index(questions)
    results = search(index, "two su")
    slugs = [q.slug for q in results]
    assert "two-sum" in slugs
    assert "two-sum-ii-input-array-is-sorted" in slugs
    assert "add-two-numbers" not in slugs[:2]


def test_digit_query_prefix_matches_id_first(questions):
    index = build_index(questions)
    results = search(index, "1")
    # Every id starting with "1", in numeric order, ahead of any fuzzy match.
    assert [q.id for q in results[:3]] == ["1", "167", "1099"]


def test_digit_query_for_specific_id(questions):
    index = build_index(questions)
    assert search(index, "167")[0].id == "167"


def test_slug_query_matches(questions):
    index = build_index(questions)
    assert search(index, "longest-substring")[0].id == "3"


def test_limit_is_respected(questions):
    index = build_index(questions)
    assert len(search(index, "two", limit=2)) == 2


def test_no_matches_returns_empty(questions):
    index = build_index(questions)
    assert search(index, "zzzzqqqqxxxx") == ()
