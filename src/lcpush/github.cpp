#include "lcpush/github.hpp"

#include <nlohmann/json.hpp>

#include "lcpush/errors.hpp"
#include "lcpush/http/curl_transport.hpp"
#include "lcpush/util/strings.hpp"
#include "lcpush/util/time.hpp"

namespace lcpush::github {

namespace {

using nlohmann::json;

std::string encode_path(const std::string& path) {
    std::string trimmed = path;
    while (!trimmed.empty() && trimmed.front() == '/') trimmed.erase(0, 1);
    return util::url_quote_path(trimmed);
}

std::string header(const http::Response& response, const std::string& name) {
    auto hit = response.headers.find(name);
    return hit == response.headers.end() ? "" : hit->second;
}

bool rate_limited(const http::Response& response) {
    return response.status == 403 && header(response, "x-ratelimit-remaining") == "0";
}

std::string rate_limit_message(const http::Response& response) {
    std::string reset = header(response, "x-ratelimit-reset");
    std::string when;
    if (!reset.empty() && util::is_all_digits(reset)) {
        std::string stamp = util::format_local_hms(std::stoll(reset));
        when = " Resets at " + stamp + ".";
    }
    return "GitHub API rate limit exceeded." + when;
}

// JSON "message" plus the first errors[].message, mirroring the Python helper.
std::string detail(const http::Response& response) {
    json payload = json::parse(response.body, nullptr, false);
    if (payload.is_discarded()) {
        std::string text = util::trim(response.body);
        if (text.size() > 200) text.resize(200);
        return text.empty() ? response.reason : text;
    }
    if (payload.is_object()) {
        std::string message;
        if (payload.contains("message")) {
            auto& raw = payload["message"];
            message = util::trim(raw.is_string() ? raw.get<std::string>() : raw.dump());
        }
        if (payload.contains("errors") && payload["errors"].is_array() &&
            !payload["errors"].empty()) {
            const auto& first = payload["errors"][0];
            if (first.is_object() && first.contains("message") &&
                first["message"].is_string() && !first["message"].get<std::string>().empty()) {
                std::string inner = first["message"];
                return message.empty() ? inner : message + " (" + inner + ")";
            }
        }
        return message.empty() ? response.reason : message;
    }
    return response.reason;
}

}  // namespace

GitHubClient::GitHubClient(std::string token, std::shared_ptr<http::HttpTransport> transport)
    : token_(std::move(token)), transport_(std::move(transport)) {
    if (transport_ == nullptr) transport_ = http::default_transport();
}

http::Response GitHubClient::request(const std::string& method, const std::string& path,
                                     const std::string& body) {
    http::Request req;
    req.method = method;
    req.url = std::string(kApiRoot) + path;
    req.headers = {
        {"Accept", "application/vnd.github+json"},
        {"X-GitHub-Api-Version", "2022-11-28"},
        {"Authorization", "Bearer " + token_},
    };
    if (!body.empty()) {
        req.headers.emplace_back("Content-Type", "application/json");
        req.body = body;
    }
    try {
        return transport_->send(req);
    } catch (const http::TransportError& exc) {
        throw GitHubError(std::string("Could not reach GitHub: ") + exc.what());
    }
}

void GitHubClient::raise_for(const http::Response& response, const std::string& repo) {
    int status = response.status;
    if (status == 401) {
        throw TokenError("GitHub token invalid or expired. Run: lcpush config reset-token");
    }
    if (rate_limited(response)) {
        throw GitHubError(rate_limit_message(response));
    }
    if (status == 403) {
        throw TokenError("GitHub denied write access to " + repo + ". " + kRequiredScope);
    }
    if (status == 404) {
        throw GitHubError("Repo not found or token lacks access: " + repo);
    }
    if (status == 422) {
        throw GitHubError("GitHub rejected the request: " + detail(response));
    }
    throw GitHubError("GitHub returned " + std::to_string(status) + ": " + detail(response));
}

RepoInfo GitHubClient::get_repo(const std::string& owner, const std::string& name) {
    std::string repo = owner + "/" + name;
    auto response = request("GET", "/repos/" + owner + "/" + name);
    if (response.status != 200) raise_for(response, repo);
    json data = json::parse(response.body, nullptr, false);
    if (data.is_discarded() || !data.is_object()) data = json::object();

    RepoInfo info;
    info.full_name = data.value("full_name", repo);
    info.default_branch = data.value("default_branch", "");
    if (info.default_branch.empty()) info.default_branch = "main";
    info.can_push = true;
    if (data.contains("permissions") && data["permissions"].is_object()) {
        info.can_push = data["permissions"].value("push", true);
    }
    return info;
}

std::optional<std::string> GitHubClient::get_file_sha(const std::string& owner,
                                                      const std::string& name,
                                                      const std::string& path,
                                                      const std::string& branch) {
    std::string repo = owner + "/" + name;
    auto response = request("GET", "/repos/" + owner + "/" + name + "/contents/" +
                                        encode_path(path) + "?ref=" + branch);
    if (response.status == 404) return std::nullopt;
    if (response.status != 200) raise_for(response, repo);
    json data = json::parse(response.body, nullptr, false);
    if (data.is_array()) {
        throw GitHubError(path + " is a directory in " + repo + ", not a file");
    }
    if (data.is_object() && data.contains("sha") && data["sha"].is_string()) {
        return data["sha"].get<std::string>();
    }
    return std::nullopt;
}

PushResult GitHubClient::put_file(const std::string& owner, const std::string& name,
                                  const std::string& path, const PutFileOptions& options) {
    std::string repo = owner + "/" + name;
    json body = {
        {"message", options.message},
        {"content", util::base64_encode(options.content)},
        {"branch", options.branch},
    };
    if (options.sha && !options.sha->empty()) body["sha"] = *options.sha;
    if (!options.author_name.empty() && !options.author_email.empty()) {
        json author = {{"name", options.author_name}, {"email", options.author_email}};
        body["author"] = author;
        body["committer"] = author;
    }

    std::string endpoint =
        "/repos/" + owner + "/" + name + "/contents/" + encode_path(path);
    auto response = request("PUT", endpoint, body.dump());

    if (response.status == 409) {
        // Someone else moved the file under us: re-fetch the sha once.
        auto fresh = get_file_sha(owner, name, path, options.branch);
        if (fresh) {
            body["sha"] = *fresh;
        } else {
            body.erase("sha");
        }
        response = request("PUT", endpoint, body.dump());
    }

    if (response.status != 200 && response.status != 201) raise_for(response, repo);

    json data = json::parse(response.body, nullptr, false);
    if (data.is_discarded() || !data.is_object()) data = json::object();
    PushResult result;
    if (data.contains("content") && data["content"].is_object()) {
        result.html_url = data["content"].value("html_url", "");
    }
    if (data.contains("commit") && data["commit"].is_object()) {
        result.commit_sha = data["commit"].value("sha", "");
    }
    result.updated = response.status == 200;
    return result;
}

}  // namespace lcpush::github
