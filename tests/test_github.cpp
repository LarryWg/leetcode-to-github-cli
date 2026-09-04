#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <nlohmann/json.hpp>

#include <ctime>

#include "helpers/fake_transport.hpp"
#include "lcpush/core/errors.hpp"
#include "lcpush/github/client.hpp"

using namespace lcpush;
using lcpush::testing::json_response;
using lcpush::testing::make_transport;
using nlohmann::json;

namespace {

constexpr const char* kToken = "ghp_secret_token_value";

github::GitHubClient client_for(testing::FakeTransport::Handler handler) {
    return github::GitHubClient(kToken, make_transport(std::move(handler)));
}

std::string header_of(const http::Request& request, const std::string& name) {
    for (const auto& [key, value] : request.headers) {
        if (key == name) return value;
    }
    return "";
}

}  // namespace

TEST_CASE("get repo returns default branch") {
    auto github = client_for([](const http::Request& request) {
        CHECK(header_of(request, "Authorization") == std::string("Bearer ") + kToken);
        CHECK(header_of(request, "X-GitHub-Api-Version") == "2022-11-28");
        CHECK(header_of(request, "Accept") == "application/vnd.github+json");
        return json_response(200, json({{"full_name", "user/solutions"},
                                        {"default_branch", "trunk"},
                                        {"permissions", {{"push", true}}}})
                                      .dump());
    });
    auto info = github.get_repo("user", "solutions");
    CHECK(info.default_branch == "trunk");
    CHECK(info.can_push);
}

TEST_CASE("repo 404 names the repo") {
    auto github = client_for([](const http::Request&) {
        return json_response(404, json({{"message", "Not Found"}}).dump());
    });
    CHECK_THROWS_WITH(github.get_repo("user", "missing"),
                      "Repo not found or token lacks access: user/missing");
}

TEST_CASE("401 points at reset token") {
    auto github = client_for([](const http::Request&) {
        return json_response(401, json({{"message", "Bad credentials"}}).dump());
    });
    CHECK_THROWS_MATCHES(github.get_repo("user", "solutions"), TokenError,
                         Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring(
                             "lcpush config reset-token")));
}

TEST_CASE("403 names the required scope") {
    auto github = client_for([](const http::Request&) {
        return json_response(403, json({{"message", "Resource not accessible"}}).dump());
    });
    CHECK_THROWS_MATCHES(github.get_repo("user", "solutions"), TokenError,
                         Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring(
                             "contents: read & write")));
}

TEST_CASE("rate limit reports reset time") {
    std::time_t reset = std::time(nullptr) + 600;
    auto github = client_for([reset](const http::Request&) {
        auto response = json_response(403, json({{"message", "rate limited"}}).dump());
        response.headers["x-ratelimit-remaining"] = "0";
        response.headers["x-ratelimit-reset"] = std::to_string(reset);
        return response;
    });
    try {
        github.get_repo("user", "solutions");
        FAIL("expected GitHubError");
    } catch (const GitHubError& exc) {
        std::string message = exc.what();
        CHECK(message.find("rate limit") != std::string::npos);
        CHECK(message.find("Resets at") != std::string::npos);
    }
}

TEST_CASE("get file sha none when absent") {
    auto github = client_for([](const http::Request&) {
        return json_response(404, json({{"message", "Not Found"}}).dump());
    });
    CHECK_FALSE(github.get_file_sha("user", "solutions", "0001-two-sum.py", "main")
                    .has_value());
}

TEST_CASE("get file sha present") {
    auto github = client_for([](const http::Request& request) {
        CHECK(request.url.find("?ref=main") != std::string::npos);
        return json_response(200, json({{"sha", "abc123"}}).dump());
    });
    auto sha = github.get_file_sha("user", "solutions", "0001-two-sum.py", "main");
    REQUIRE(sha.has_value());
    CHECK(*sha == "abc123");
}

TEST_CASE("put new file sends base64 and no sha") {
    json captured;
    auto github = client_for([&captured](const http::Request& request) {
        captured = json::parse(request.body);
        return json_response(
            201, json({{"content",
                        {{"html_url", "https://github.com/user/solutions/blob/main/x.py"}}},
                       {"commit", {{"sha", "deadbeef"}}}})
                     .dump());
    });
    auto result = github.put_file("user", "solutions", "0001-two-sum.py",
                                  {.content = "print(1)\n",
                                   .message = "Add 1. Two Sum (Python3)",
                                   .branch = "main"});
    CHECK(captured["content"] == "cHJpbnQoMSkK");  // base64 of "print(1)\n"
    CHECK(captured["branch"] == "main");
    CHECK_FALSE(captured.contains("sha"));
    CHECK(result.updated == false);
    CHECK(result.html_url.ends_with("x.py"));
}

TEST_CASE("put file rejects a malformed success response") {
    auto github = client_for([](const http::Request&) {
        return json_response(201, json({{"content", json::object()},
                                        {"commit", json::object()}}).dump());
    });
    CHECK_THROWS_MATCHES(
        github.put_file("user", "solutions", "p.py",
                        {.content = "x\n", .message = "m", .branch = "main"}),
        GitHubError,
        Catch::Matchers::MessageMatches(
            Catch::Matchers::ContainsSubstring("malformed success response")));
}

TEST_CASE("put existing file includes sha and author") {
    json captured;
    auto github = client_for([&captured](const http::Request& request) {
        captured = json::parse(request.body);
        return json_response(
            200, json({{"content", {{"html_url", "u"}}}, {"commit", {{"sha", "s"}}}}).dump());
    });
    auto result = github.put_file("user", "solutions", "0001-two-sum.py",
                                  {.content = "x\n",
                                   .message = "Update",
                                   .branch = "main",
                                   .sha = "old",
                                   .author_name = "Larry",
                                   .author_email = "larry@example.com"});
    CHECK(captured["sha"] == "old");
    CHECK(captured["author"] == json({{"name", "Larry"}, {"email", "larry@example.com"}}));
    CHECK(result.updated == true);
}

TEST_CASE("conflict refetches sha once then succeeds") {
    std::vector<std::string> calls;
    auto github = client_for([&calls](const http::Request& request) {
        calls.push_back(request.method);
        if (request.method == "PUT") {
            int puts = 0;
            for (const auto& method : calls)

                if (method == "PUT") ++puts;
            if (puts == 1) {
                return json_response(409, json({{"message", "conflict"}}).dump());
            }
            CHECK(json::parse(request.body)["sha"] == "fresh");
            return json_response(
                200,
                json({{"content", {{"html_url", "u"}}}, {"commit", {{"sha", "s"}}}}).dump());
        }
        return json_response(200, json({{"sha", "fresh"}}).dump());
    });
    auto result = github.put_file(
        "user", "solutions", "p.py",
        {.content = "x\n", .message = "m", .branch = "main", .sha = "stale"});
    CHECK(result.html_url == "u");
    CHECK(calls == std::vector<std::string>{"PUT", "GET", "PUT"});
}

TEST_CASE("conflict twice fails clearly") {
    auto github = client_for([](const http::Request& request) {
        if (request.method == "PUT") {
            return json_response(409, json({{"message", "still conflicting"}}).dump());
        }
        return json_response(200, json({{"sha", "fresh"}}).dump());
    });
    CHECK_THROWS_MATCHES(
        github.put_file("user", "solutions", "p.py",
                        {.content = "x\n", .message = "m", .branch = "main", .sha = "stale"}),
        GitHubError,
        Catch::Matchers::MessageMatches(Catch::Matchers::ContainsSubstring("409")));
}

TEST_CASE("network failure is a single line") {
    auto github = client_for([](const http::Request&) -> http::Response {
        throw http::TransportError("no route to host");
    });
    CHECK_THROWS_MATCHES(github.get_repo("user", "solutions"), GitHubError,
                         Catch::Matchers::MessageMatches(Catch::Matchers::StartsWith(
                             "Could not reach GitHub")));
}

TEST_CASE("errors never leak the token") {
    auto github = client_for([](const http::Request&) {
        return json_response(500, json({{"message", "server exploded"}}).dump());
    });
    try {
        github.get_repo("user", "solutions");
        FAIL("expected GitHubError");
    } catch (const GitHubError& exc) {
        CHECK(std::string(exc.what()).find(kToken) == std::string::npos);
    }
}

TEST_CASE("paths are url encoded") {
    std::string seen;
    auto github = client_for([&seen](const http::Request& request) {
        seen = request.url;
        return json_response(404, json({{"message", "Not Found"}}).dump());
    });
    github.get_file_sha("user", "solutions", "sub dir/0001-two-sum.py", "main");
    CHECK(seen.find("sub%20dir/0001-two-sum.py") != std::string::npos);
}

TEST_CASE("branch query values are url encoded") {
    std::string seen;
    auto github = client_for([&seen](const http::Request& request) {
        seen = request.url;
        return json_response(404, json({{"message", "Not Found"}}).dump());
    });
    github.get_file_sha("user", "solutions", "solution.py", "feature/a+b #1");
    CHECK(seen.find("?ref=feature%2Fa%2Bb%20%231") != std::string::npos);
}
