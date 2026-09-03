#include <catch2/catch_test_macros.hpp>

#include "helpers/fixtures.hpp"
#include "lcpush/detect.hpp"
#include "lcpush/render.hpp"

using namespace lcpush;

namespace {

const detect::Language& py() { return *detect::by_key("python3"); }
const detect::Language& cpp() { return *detect::by_key("cpp"); }

}  // namespace

TEST_CASE("target path zero pads to four") {
    CHECK(render::target_path("", testing::two_sum(), py()) == "0001-two-sum.py");
}

TEST_CASE("target path with prefix") {
    CHECK(render::target_path("solutions/", testing::two_sum(), py()) ==
          "solutions/0001-two-sum.py");
}

TEST_CASE("target path for long slug") {
    Question question{"167", "Two Sum II", "two-sum-ii-input-array-is-sorted", "Medium"};
    CHECK(render::target_path("", question, cpp()) ==
          "0167-two-sum-ii-input-array-is-sorted.cpp");
}

TEST_CASE("non numeric ids are used verbatim") {
    Question question{"LCP 01", "Guess Numbers", "guess-numbers", "Easy"};
    CHECK(render::filename(question, py()) == "LCP 01-guess-numbers.py");
}

TEST_CASE("message variables") {
    auto variables = render::message_vars(testing::two_sum(), py(), 24, "solutions/");
    CHECK(variables["padded_id"] == "0001");
    CHECK(variables["language"] == "Python3");
    CHECK(variables["ext"] == ".py");
    CHECK(variables["difficulty"] == "Easy");
    CHECK(variables["filename"] == "0001-two-sum.py");
    CHECK(variables["path"] == "solutions/0001-two-sum.py");
    CHECK(variables["lines"] == "24");
}

TEST_CASE("default add message") {
    auto rendered = render::render_message(
        testing::two_sum(), py(),
        {.lines = 24,
         .updating = false,
         .message_template = "Add {id}. {title} ({language})",
         .update_template = "Update {id}. {title} ({language})"});
    CHECK(rendered.text == "Add 1. Two Sum (Python3)");
    CHECK_FALSE(rendered.warning.has_value());
}

TEST_CASE("update template used when overwriting") {
    auto rendered = render::render_message(
        testing::two_sum(), py(),
        {.lines = 24,
         .updating = true,
         .message_template = "Add {id}. {title} ({language})",
         .update_template = "Update {id}. {title} ({language})"});
    CHECK(rendered.text == "Update 1. Two Sum (Python3)");
}

TEST_CASE("custom template with every variable") {
    auto rendered = render::render_message(
        testing::two_sum(), py(),
        {.lines = 24,
         .updating = false,
         .message_template = "{padded_id} {slug} {difficulty} {filename} {lines}{ext}",
         .update_template = "u"});
    CHECK(rendered.text == "0001 two-sum Easy 0001-two-sum.py 24.py");
}

TEST_CASE("bad template falls back and warns") {
    auto rendered = render::render_message(testing::two_sum(), py(),
                                           {.lines = 24,
                                            .updating = false,
                                            .message_template = "Add {titel}",
                                            .update_template = "Update {id}"});
    CHECK(rendered.text == "Add 1. Two Sum (Python3)");
    REQUIRE(rendered.warning.has_value());
    CHECK(rendered.warning->find("invalid") != std::string::npos);
}

TEST_CASE("empty template falls back") {
    auto rendered = render::render_message(testing::two_sum(), py(),
                                           {.lines = 1,
                                            .updating = false,
                                            .message_template = "   ",
                                            .update_template = "Update {id}"});
    CHECK(rendered.text == "Add 1. Two Sum (Python3)");
    CHECK(rendered.warning.has_value());
}

TEST_CASE("unknown template fields") {
    auto variables = render::message_vars(testing::two_sum(), py(), 1);
    CHECK(render::unknown_template_fields("{id} {titel}", variables) ==
          std::vector<std::string>{"titel"});
    CHECK(render::unknown_template_fields("{id} {title}", variables).empty());
}

TEST_CASE("clean message preserves body") {
    CHECK(render::clean_message("subject   \n\nbody line  \n") == "subject\n\nbody line");
}

TEST_CASE("subject warning over 72 chars") {
    CHECK(render::subject_warning(std::string(73, 'x')).has_value());
    CHECK_FALSE(render::subject_warning(std::string(72, 'x')).has_value());
    CHECK_FALSE(render::subject_warning("short\n" + std::string(200, 'y')).has_value());
}
