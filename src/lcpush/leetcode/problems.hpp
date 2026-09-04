// The LeetCode problem set: fetch over GraphQL and a local JSON cache with a
// stale-while-revalidate policy. A cache always beats a blocking fetch: only
// the very first run (no cache) or an explicit --refresh waits on the network.
#pragma once

#include <ctime>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "lcpush/http/transport.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush {

struct Question {
    std::string id;
    std::string title;
    std::string slug;
    std::string difficulty;
    bool paid = false;

    std::string display() const { return id + ". " + title; }

    // Zero-pad numeric ids to 4 digits, use rare non-numeric ids verbatim.
    std::string padded_id() const {
        return util::is_all_digits(id) ? util::zfill(id, 4) : id;
    }

    bool operator==(const Question&) const = default;
};

namespace problems {

inline constexpr const char* kGraphqlUrl = "https://leetcode.com/graphql";
inline constexpr double kPageDelaySeconds = 0.2;

using SleepFn = std::function<void(double seconds)>;
using NotifyFn = std::function<void(const std::string&)>;

// Fetch the whole public problem set, paginating on skip. Degrades limit
// 500 -> 100 and categorySlug -> "" when an attempt is rejected. Throws
// LeetCodeError when every combination fails.
std::vector<Question> fetch_all(http::HttpTransport& transport,
                                const SleepFn& sleep = nullptr,
                                const std::function<bool()>& should_abort = nullptr);

// Atomic cache write (tmp + rename). Timestamp defaults to now.
std::filesystem::path save_cache(const std::vector<Question>& questions,
                                 const std::filesystem::path& path,
                                 std::optional<std::time_t> now = std::nullopt);
std::filesystem::path save_cache(const std::vector<Question>& questions,
                                 std::optional<std::time_t> now = std::nullopt);

struct CacheContent {
    std::optional<std::time_t> fetched_at;
    std::vector<Question> questions;
};

// (fetched_at, questions), or an empty result when there is no usable cache.
CacheContent load_cache();
CacheContent load_cache(const std::filesystem::path& path);

bool is_stale(std::optional<std::time_t> fetched_at, int ttl_days,
              std::optional<std::time_t> now = std::nullopt);

// Holds the background refresh thread. Destroying it requests a stop (the
// transfer aborts within one progress tick) and joins.
class RefreshHandle {
  public:
    RefreshHandle() = default;
    explicit RefreshHandle(std::jthread thread) : thread_(std::move(thread)) {}

    bool active() const { return thread_.joinable(); }
    void join() { if (thread_.joinable()) thread_.join(); }

  private:
    std::jthread thread_;
};

// Refresh the cache on a background thread so startup never waits on it.
// Failures are silently dropped: the stale cache stays in place.
RefreshHandle spawn_refresh(const std::filesystem::path& target,
                            const http::TransportFactory& factory);

struct GetQuestionsOptions {
    int ttl_days = 7;
    bool refresh = false;
    std::optional<std::filesystem::path> path;
    http::TransportFactory transport_factory;
    NotifyFn warn;
    NotifyFn info;
    std::optional<std::time_t> now;
};

// Cached problem set. A stale cache is served immediately and refreshed in
// the background when refresh_out is provided. Only a missing cache or
// refresh=true blocks on the fetch.
std::vector<Question> get_questions(const GetQuestionsOptions& options = {},
                                    RefreshHandle* refresh_out = nullptr);

}  // namespace problems
}  // namespace lcpush
