from __future__ import annotations

from lcpush.detect import BY_KEY
from lcpush.problems import Question
from lcpush.render import (
    clean_message,
    filename,
    message_vars,
    render_message,
    subject_warning,
    target_path,
    unknown_template_fields,
)

PY = BY_KEY["python3"]
CPP = BY_KEY["cpp"]


def test_target_path_zero_pads_to_four(two_sum):
    assert target_path("", two_sum, PY) == "0001-two-sum.py"


def test_target_path_with_prefix(two_sum):
    assert target_path("solutions/", two_sum, PY) == "solutions/0001-two-sum.py"


def test_target_path_for_long_slug():
    question = Question("167", "Two Sum II", "two-sum-ii-input-array-is-sorted", "Medium")
    assert target_path("", question, CPP) == "0167-two-sum-ii-input-array-is-sorted.cpp"


def test_non_numeric_ids_are_used_verbatim():
    question = Question("LCP 01", "Guess Numbers", "guess-numbers", "Easy")
    assert filename(question, PY) == "LCP 01-guess-numbers.py"


def test_message_variables(two_sum):
    variables = message_vars(two_sum, PY, lines=24, prefix="solutions/")
    assert variables["padded_id"] == "0001"
    assert variables["language"] == "Python3"
    assert variables["ext"] == ".py"
    assert variables["difficulty"] == "Easy"
    assert variables["filename"] == "0001-two-sum.py"
    assert variables["path"] == "solutions/0001-two-sum.py"
    assert variables["lines"] == "24"


def test_default_add_message(two_sum):
    rendered = render_message(
        two_sum,
        PY,
        lines=24,
        updating=False,
        message_template="Add {id}. {title} ({language})",
        update_template="Update {id}. {title} ({language})",
    )
    assert rendered.text == "Add 1. Two Sum (Python3)"
    assert rendered.warning is None


def test_update_template_used_when_overwriting(two_sum):
    rendered = render_message(
        two_sum,
        PY,
        lines=24,
        updating=True,
        message_template="Add {id}. {title} ({language})",
        update_template="Update {id}. {title} ({language})",
    )
    assert rendered.text == "Update 1. Two Sum (Python3)"


def test_custom_template_with_every_variable(two_sum):
    rendered = render_message(
        two_sum,
        PY,
        lines=24,
        updating=False,
        message_template="{padded_id} {slug} {difficulty} {filename} {lines}{ext}",
        update_template="u",
    )
    assert rendered.text == "0001 two-sum Easy 0001-two-sum.py 24.py"


def test_bad_template_falls_back_and_warns(two_sum):
    rendered = render_message(
        two_sum,
        PY,
        lines=24,
        updating=False,
        message_template="Add {titel}",
        update_template="Update {id}",
    )
    assert rendered.text == "Add 1. Two Sum (Python3)"
    assert rendered.warning is not None and "invalid" in rendered.warning


def test_empty_template_falls_back(two_sum):
    rendered = render_message(
        two_sum,
        PY,
        lines=1,
        updating=False,
        message_template="   ",
        update_template="Update {id}",
    )
    assert rendered.text == "Add 1. Two Sum (Python3)"
    assert rendered.warning is not None


def test_unknown_template_fields(two_sum):
    variables = message_vars(two_sum, PY, lines=1)
    assert unknown_template_fields("{id} {titel}", variables) == ("titel",)
    assert unknown_template_fields("{id} {title}", variables) == ()


def test_clean_message_preserves_body():
    assert clean_message("subject   \n\nbody line  \n") == "subject\n\nbody line"


def test_subject_warning_over_72_chars():
    assert subject_warning("x" * 73) is not None
    assert subject_warning("x" * 72) is None
    assert subject_warning("short\n" + "y" * 200) is None
