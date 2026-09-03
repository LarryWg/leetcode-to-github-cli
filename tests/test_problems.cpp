#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

#include "helpers/fake_transport.hpp"
#include "helpers/fixtures.hpp"
#include "lcpush/errors.hpp"
#include "lcpush/paths.hpp"
#include "lcpush/problems.hpp"
#include "lcpush/util/time.hpp"

using namespace lcpush;
using lcpush::testing::IsolatedDirs;
using lcpush::testing::json_response;
using nlohmann::json;

namespace {

// 2026-07-27T10:00:00Z, matching the Python test's NOW.
const std::time_t kNow = *util::parse_iso_utc("2026-07-27T10:00:00Z");
constexpr std::time_t kDay = 24 * 3600;

json page(int total, int start, int count) {
    json questions = json::array();
    for (int index = start; index < start + count; ++index) {
        questions.push_back({{"frontendQuestionId", std::to_string(index)},
                             {"title", "Problem " + std::to_string(index)},
                             {"titleSlug", "problem-" + std::to_string(index)},
                             {"difficulty", "Easy"},
                             {"paidOnly", false}});
    }
    return {{"data",
             {{"problemsetQuestionList", {{"total", total}, {"questions", questions}}}}}};
}

http::TransportFactory factory_for(testing::FakeTransport::Handler handler) {
    return [handler]() -> std::unique_ptr<http::HttpTransport> {
        return std::make_unique<testing::FakeTransport>(handler);
    };
}

auto no_sleep = [](double) {};

}  // namespace

TEST_CASE("fetch paginates until total") {
    std::vector<int> calls;
    testing::FakeTransport transport([&calls](const http::Request& request) {
        int skip = json::parse(request.body)["variables"]["skip"];
        calls.push_back(skip);
        int start = skip + 1;
        int count = std::min(500, 1200 - skip);
        return json_response(200, page(1200, start, count).dump());
    });
    auto questions = problems::fetch_all(transport, no_sleep);
    CHECK(questions.size() == 1200);
    CHECK(calls == std::vector<int>{0, 500, 1000});
    CHECK(questions[0].id == "1");
}

TEST_CASE("fetch advances by actual page size when the server caps it") {
    std::vector<int> skips;
    testing::FakeTransport transport([&skips](const http::Request& request) {
        int skip = json::parse(request.body)["variables"]["skip"];
        skips.push_back(skip);
        int count = std::max(0, std::min(100, 250 - skip));
        return json_response(200, page(250, skip + 1, count).dump());
    });
    auto questions = problems::fetch_all(transport, no_sleep);
    CHECK(skips == std::vector<int>{0, 100, 200});
    REQUIRE(questions.size() == 250);
    CHECK(questions.front().id == "1");
    CHECK(questions.back().id == "250");
}

TEST_CASE("fetch degrades page size then category") {
    std::vector<std::pair<std::string, int>> attempts;
    testing::FakeTransport transport([&attempts](const http::Request& request) {
        auto variables = json::parse(request.body)["variables"];
        attempts.emplace_back(variables["categorySlug"], variables["limit"]);
        if (variables["limit"] == 500) {
            return json_response(400,
                                 json({{"errors", {{{"message", "limit too high"}}}}}).dump());
        }
        if (variables["categorySlug"] == "all-code-essentials") {
            return json_response(200,
                                 json({{"errors", {{{"message", "bad category"}}}}}).dump());
        }
        return json_response(200, page(2, 1, 2).dump());
    });
    auto questions = problems::fetch_all(transport, no_sleep);
    CHECK(questions.size() == 2);
    bool degraded = false;
    for (const auto& [category, limit] : attempts) {
        if (category.empty() && limit == 100) degraded = true;
    }
    CHECK(degraded);
}

TEST_CASE("fetch raises when everything fails") {
    testing::FakeTransport transport([](const http::Request&) {
        http::Response response;
        response.status = 500;
        response.body = "boom";
        return response;
    });
    CHECK_THROWS_AS(problems::fetch_all(transport, no_sleep), LeetCodeError);
}

TEST_CASE("cache round trip") {
    IsolatedDirs dirs;
    auto questions = testing::questions();
    auto path = problems::save_cache(questions, kNow);
    auto content = problems::load_cache(path);
    REQUIRE(content.fetched_at.has_value());
    CHECK(*content.fetched_at == kNow);
    CHECK(content.questions == questions);

    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    json record = json::parse(buffer.str());
    CHECK(record["fetched_at"] == "2026-07-27T10:00:00Z");
}

TEST_CASE("load cache missing and corrupt") {
    IsolatedDirs dirs;
    auto missing = problems::load_cache();
    CHECK_FALSE(missing.fetched_at.has_value());
    CHECK(missing.questions.empty());

    auto path = paths::problems_cache_file();
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << "{not json";
    auto corrupt = problems::load_cache(path);
    CHECK_FALSE(corrupt.fetched_at.has_value());
    CHECK(corrupt.questions.empty());
}

TEST_CASE("is stale") {
    CHECK(problems::is_stale(std::nullopt, 7));
    CHECK_FALSE(problems::is_stale(kNow, 7, kNow + 6 * kDay));
    CHECK(problems::is_stale(kNow, 7, kNow + 8 * kDay));
    CHECK(problems::is_stale(kNow, 0, kNow));
}

TEST_CASE("get questions uses fresh cache") {
    IsolatedDirs dirs;
    auto questions = testing::questions();
    problems::save_cache(questions, kNow);
    bool network_used = false;
    auto factory = [&network_used]() -> std::unique_ptr<http::HttpTransport> {
        network_used = true;
        return std::make_unique<testing::FakeTransport>(
            [](const http::Request&) { return json_response(200, "{}"); });
    };
    auto result = problems::get_questions(
        {.ttl_days = 7, .transport_factory = factory, .now = kNow});
    CHECK(result == questions);
    CHECK_FALSE(network_used);
}

TEST_CASE("get questions serves stale cache and refreshes in background") {
    IsolatedDirs dirs;
    auto questions = testing::questions();
    problems::save_cache(questions, kNow - 30 * kDay);
    problems::RefreshHandle refresh;
    auto result = problems::get_questions(
        {.ttl_days = 7,
         .transport_factory = factory_for([](const http::Request&) {
             return json_response(200, page(2, 1, 2).dump());
         }),
         .now = kNow},
        &refresh);
    // The stale cache is returned immediately, not the 2-question fetch result.
    CHECK(result == questions);
    CHECK(refresh.active());
    refresh.join();
    CHECK(problems::load_cache().questions.size() == 2);
}

TEST_CASE("background refresh failure keeps the stale cache") {
    IsolatedDirs dirs;
    auto questions = testing::questions();
    problems::save_cache(questions, kNow - 30 * kDay);
    auto handle = problems::spawn_refresh(
        paths::problems_cache_file(), factory_for([](const http::Request&) -> http::Response {
            throw http::TransportError("offline");
        }));
    handle.join();
    CHECK(problems::load_cache().questions == questions);
}

TEST_CASE("get questions without cache or network") {
    IsolatedDirs dirs;
    auto factory = factory_for(
        [](const http::Request&) -> http::Response { throw http::TransportError("offline"); });
    CHECK_THROWS_MATCHES(
        problems::get_questions({.ttl_days = 7, .transport_factory = factory, .now = kNow}),
        LeetCodeError,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("no cached problem list")));
}

TEST_CASE("get questions first run announces the fetch") {
    IsolatedDirs dirs;
    std::vector<std::string> notes;
    problems::get_questions({.ttl_days = 7,
                             .transport_factory = factory_for([](const http::Request&) {
                                 return json_response(200, page(2, 1, 2).dump());
                             }),
                             .info = [&notes](const std::string& note) { notes.push_back(note); },
                             .now = kNow});
    REQUIRE_FALSE(notes.empty());
    CHECK(notes[0].find("first run") != std::string::npos);
}

TEST_CASE("get questions explicit refresh falls back to cache with a warning") {
    IsolatedDirs dirs;
    auto questions = testing::questions();
    problems::save_cache(questions, kNow);
    std::vector<std::string> warnings;
    auto result = problems::get_questions(
        {.ttl_days = 7,
         .refresh = true,
         .transport_factory = factory_for([](const http::Request&) -> http::Response {
             throw http::TransportError("offline");
         }),
         .warn = [&warnings](const std::string& note) { warnings.push_back(note); },
         .now = kNow});
    CHECK(result == questions);
    REQUIRE_FALSE(warnings.empty());
    CHECK(warnings[0].find("using cache") != std::string::npos);
}

TEST_CASE("get questions refresh writes cache") {
    IsolatedDirs dirs;
    auto result = problems::get_questions(
        {.ttl_days = 7,
         .refresh = true,
         .transport_factory = factory_for([](const http::Request&) {
             return json_response(200, page(2, 1, 2).dump());
         }),
         .now = kNow});
    CHECK(result.size() == 2);
    CHECK(std::filesystem::exists(paths::problems_cache_file()));
}

TEST_CASE("padded id handles non numeric") {
    CHECK(Question{"42", "T", "t", "Easy"}.padded_id() == "0042");
    CHECK(Question{"LCP 01", "T", "t", "Easy"}.padded_id() == "LCP 01");
}
