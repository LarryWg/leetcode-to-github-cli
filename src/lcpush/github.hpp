// GitHub Contents API client. Every non-2xx response becomes a single-line
// LcpushError. The token appears in exactly one place, the Authorization
// header, and never in an error message.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "lcpush/http/transport.hpp"

namespace lcpush::github {

inline constexpr const char* kApiRoot = "https://api.github.com";
inline constexpr const char* kRequiredScope =
    "Token needs the `repo` scope (classic) or `contents: read & write` (fine-grained).";

struct RepoInfo {
    std::string full_name;
    std::string default_branch;
    bool can_push = true;
};

struct PushResult {
    std::string html_url;
    std::string commit_sha;
    bool updated = false;
};

struct PutFileOptions {
    std::string content;
    std::string message;
    std::string branch;
    std::optional<std::string> sha;
    std::string author_name;
    std::string author_email;
};

// The three endpoints lcpush needs, as a seam so session tests can fake it.
class Api {
  public:
    virtual ~Api() = default;
    virtual RepoInfo get_repo(const std::string& owner, const std::string& name) = 0;
    virtual std::optional<std::string> get_file_sha(const std::string& owner,
                                                    const std::string& name,
                                                    const std::string& path,
                                                    const std::string& branch) = 0;
    virtual PushResult put_file(const std::string& owner, const std::string& name,
                                const std::string& path,
                                const PutFileOptions& options) = 0;
};

// Thin wrapper over the three endpoints lcpush needs.
class GitHubClient final : public Api {
  public:
    // Owns a CurlTransport unless one is injected for tests.
    explicit GitHubClient(std::string token,
                          std::shared_ptr<http::HttpTransport> transport = nullptr);

    // Verify access and read the default branch (setup flow).
    RepoInfo get_repo(const std::string& owner, const std::string& name) override;

    // The blob sha when the file already exists, else nullopt (404).
    std::optional<std::string> get_file_sha(const std::string& owner,
                                            const std::string& name,
                                            const std::string& path,
                                            const std::string& branch) override;

    // Create or update a file, retrying once on a 409 sha conflict.
    PushResult put_file(const std::string& owner, const std::string& name,
                        const std::string& path, const PutFileOptions& options) override;

  private:
    http::Response request(const std::string& method, const std::string& path,
                           const std::string& body = "");
    [[noreturn]] void raise_for(const http::Response& response, const std::string& repo);

    std::string token_;
    std::shared_ptr<http::HttpTransport> transport_;
};

}  // namespace lcpush::github
