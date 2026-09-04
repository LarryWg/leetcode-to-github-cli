#include "lcpush/leetcode/problems.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

#include "lcpush/core/errors.hpp"
#include "lcpush/http/curl_transport.hpp"
#include "lcpush/platform/paths.hpp"
#include "lcpush/util/time.hpp"

namespace lcpush::problems {

namespace {

using nlohmann::json;

constexpr const char* kUserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

constexpr const char* kQuery = R"(
query problemsetQuestionList($categorySlug: String, $limit: Int, $skip: Int, $filters: QuestionListFilterInput) {
  problemsetQuestionList: questionList(
    categorySlug: $categorySlug
    limit: $limit
    skip: $skip
    filters: $filters
  ) {
    total: totalNum
    questions: data {
      frontendQuestionId: questionFrontendId
      title
      titleSlug
      difficulty
      paidOnly: isPaidOnly
    }
  }
}
)";

constexpr const char* kCategorySlugs[] = {"all-code-essentials", ""};
constexpr int kPageSizes[] = {500, 100};

std::string json_string(const json& value, const char* key,
                        const std::string& fallback = "") {
    if (!value.is_object() || !value.contains(key)) return fallback;
    const auto& raw = value[key];
    if (raw.is_string()) return raw.get<std::string>();
    if (raw.is_null()) return fallback;
    return raw.dump();
}

Question to_question(const json& raw) {
    Question question;
    question.id = util::trim(json_string(raw, "frontendQuestionId"));
    question.title = util::trim(json_string(raw, "title"));
    question.slug = util::trim(json_string(raw, "titleSlug"));
    question.difficulty = json_string(raw, "difficulty");
    if (question.difficulty.empty()) question.difficulty = "Unknown";
    question.paid = raw.is_object() && raw.value("paidOnly", false);
    return question;
}

struct Page {
    int total = 0;
    std::vector<Question> questions;
};

Page fetch_page(http::HttpTransport& transport, const std::string& category, int limit,
                int skip, const std::function<bool()>& should_abort) {
    json body = {
        {"query", kQuery},
        {"variables",
         {{"categorySlug", category},
          {"skip", skip},
          {"limit", limit},
          {"filters", json::object()}}},
    };
    http::Request request;
    request.method = "POST";
    request.url = kGraphqlUrl;
    request.headers = {
        {"Content-Type", "application/json"},
        {"Referer", "https://leetcode.com/problemset/all/"},
        {"User-Agent", kUserAgent},
    };
    request.body = body.dump();

    http::Response response = transport.send(request, should_abort);
    if (response.status < 200 || response.status >= 300) {
        throw LeetCodeError("LeetCode returned " + std::to_string(response.status));
    }
    json payload = json::parse(response.body, nullptr, false);
    if (payload.is_discarded()) {
        throw LeetCodeError("LeetCode returned invalid JSON");
    }
    if (payload.contains("errors") && payload["errors"].is_array() &&
        !payload["errors"].empty()) {
        std::string messages;
        for (const auto& err : payload["errors"]) {
            if (!messages.empty()) messages += "; ";
            messages += err.is_object() ? json_string(err, "message", err.dump()) : err.dump();
        }
        throw LeetCodeError("LeetCode rejected the query: " + messages);
    }
    const json& data = payload.contains("data") ? payload["data"] : json();
    if (!data.is_object() || !data.contains("problemsetQuestionList") ||
        data["problemsetQuestionList"].is_null()) {
        throw LeetCodeError("LeetCode returned no problem list");
    }
    const json& block = data["problemsetQuestionList"];

    Page page;
    page.total = block.value("total", 0);
    if (block.contains("questions") && block["questions"].is_array()) {
        for (const auto& item : block["questions"]) {
            page.questions.push_back(to_question(item));
        }
    }
    return page;
}

void default_sleep(double seconds) {
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}

}  // namespace

std::vector<Question> fetch_all(http::HttpTransport& transport, const SleepFn& sleep,
                                const std::function<bool()>& should_abort) {
    const SleepFn& pause = sleep ? sleep : SleepFn(default_sleep);
    std::string last_error;
    for (const char* category : kCategorySlugs) {
        for (int limit : kPageSizes) {
            try {
                std::vector<Question> collected;
                int skip = 0;
                Page page = fetch_page(transport, category, limit, skip, should_abort);
                int total = page.total;
                collected = std::move(page.questions);
                // Advance by what the server actually returned, not by what we
                // asked for: LeetCode silently caps pages at 100 however large
                // limit is, and striding by limit would skip most problems.
                size_t last_size = collected.size();
                while (last_size > 0 && collected.size() < static_cast<size_t>(total)) {
                    skip += static_cast<int>(last_size);
                    pause(kPageDelaySeconds);
                    Page next = fetch_page(transport, category, limit, skip, should_abort);
                    last_size = next.questions.size();
                    collected.insert(collected.end(), next.questions.begin(),
                                     next.questions.end());
                }
                if (!collected.empty()) return collected;
                last_error = "LeetCode returned an empty problem list";
            } catch (const LeetCodeError& exc) {
                last_error = exc.what();
            } catch (const http::TransportError& exc) {
                last_error = exc.what();
            }
        }
    }
    throw LeetCodeError("Could not fetch the LeetCode problem list: " + last_error);
}

std::filesystem::path save_cache(const std::vector<Question>& questions,
                                 const std::filesystem::path& path,
                                 std::optional<std::time_t> now) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::time_t stamp = now ? *now : std::time(nullptr);

    json record;
    record["fetched_at"] = util::format_iso_utc(stamp);
    json list = json::array();
    for (const Question& q : questions) {
        list.push_back({{"id", q.id},
                        {"title", q.title},
                        {"slug", q.slug},
                        {"difficulty", q.difficulty},
                        {"paid", q.paid}});
    }
    record["questions"] = std::move(list);

    // Atomic write: the background refresh must never leave a torn cache.
    std::filesystem::path scratch = path;
    scratch += ".tmp";
    {
        std::ofstream out(scratch);
        out << record.dump();
        if (!out.good()) {
            throw LeetCodeError("Could not write the problems cache at " + path.string());
        }
    }
    std::filesystem::rename(scratch, path, ec);
    if (ec) {
        throw LeetCodeError("Could not write the problems cache at " + path.string());
    }
    return path;
}

std::filesystem::path save_cache(const std::vector<Question>& questions,
                                 std::optional<std::time_t> now) {
    return save_cache(questions, paths::problems_cache_file(), now);
}

CacheContent load_cache() { return load_cache(paths::problems_cache_file()); }

CacheContent load_cache(const std::filesystem::path& path) {
    CacheContent content;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return content;
    std::ifstream in(path);
    if (!in.good()) return content;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    json record = json::parse(buffer.str(), nullptr, false);
    if (record.is_discarded() || !record.is_object()) return content;

    if (record.contains("fetched_at") && record["fetched_at"].is_string()) {
        content.fetched_at = util::parse_iso_utc(record["fetched_at"]);
    }
    if (record.contains("questions") && record["questions"].is_array()) {
        for (const auto& item : record["questions"]) {
            if (!item.is_object()) continue;
            std::string id = json_string(item, "id");
            std::string slug = json_string(item, "slug");
            if (id.empty() || slug.empty()) continue;
            Question question;
            question.id = id;
            question.title = json_string(item, "title");
            question.slug = slug;
            question.difficulty = json_string(item, "difficulty", "Unknown");
            question.paid = item.value("paid", false);
            content.questions.push_back(std::move(question));
        }
    }
    return content;
}

bool is_stale(std::optional<std::time_t> fetched_at, int ttl_days,
              std::optional<std::time_t> now) {
    if (!fetched_at) return true;
    if (ttl_days <= 0) return true;
    std::time_t current = now ? *now : std::time(nullptr);
    return (current - *fetched_at) > static_cast<std::time_t>(ttl_days) * 24 * 3600;
}

RefreshHandle spawn_refresh(const std::filesystem::path& target,
                            const http::TransportFactory& factory) {
    http::TransportFactory make = factory ? factory : http::default_transport;
    return RefreshHandle(std::jthread([target, make](std::stop_token stop) {
        try {
            auto transport = make();
            auto should_abort = [&stop] { return stop.stop_requested(); };
            auto fresh = fetch_all(*transport, nullptr, should_abort);
            if (stop.stop_requested()) return;
            save_cache(fresh, target);
        } catch (const std::exception&) {
            // Failures are dropped: the stale cache stays, the next run retries.
        }
    }));
}

std::vector<Question> get_questions(const GetQuestionsOptions& options,
                                    RefreshHandle* refresh_out) {
    std::filesystem::path target =
        options.path ? *options.path : paths::problems_cache_file();
    CacheContent cached = load_cache(target);
    if (!cached.questions.empty() && !options.refresh) {
        if (is_stale(cached.fetched_at, options.ttl_days, options.now)) {
            RefreshHandle handle = spawn_refresh(target, options.transport_factory);
            if (refresh_out != nullptr) {
                *refresh_out = std::move(handle);
            }
        }
        return cached.questions;
    }

    if (cached.questions.empty() && options.info) {
        options.info("Fetching the LeetCode problem list (first run only, ~30s)...");
    }
    http::TransportFactory make =
        options.transport_factory ? options.transport_factory : http::default_transport;
    std::vector<Question> fresh;
    try {
        auto transport = make();
        fresh = fetch_all(*transport);
    } catch (const std::exception& exc) {
        if (!cached.questions.empty()) {
            if (options.warn) {
                options.warn("Could not refresh the LeetCode problem list (" +
                             std::string(exc.what()) + "); using cache.");
            }
            return cached.questions;
        }
        throw LeetCodeError(
            "Could not reach LeetCode and no cached problem list. "
            "Check your connection and retry.");
    }
    save_cache(fresh, target, options.now);
    return fresh;
}

}  // namespace lcpush::problems
