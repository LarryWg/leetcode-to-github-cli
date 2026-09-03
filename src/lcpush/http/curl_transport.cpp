#include "lcpush/http/curl_transport.hpp"

#include <curl/curl.h>

#include <mutex>

#include "lcpush/util/strings.hpp"

namespace lcpush::http {

namespace {

void global_init_once() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

size_t write_body(char* data, size_t size, size_t count, void* target) {
    static_cast<std::string*>(target)->append(data, size * count);
    return size * count;
}

size_t write_header(char* data, size_t size, size_t count, void* target) {
    auto* response = static_cast<Response*>(target);
    std::string line(data, size * count);
    size_t colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = util::to_lower(util::trim(line.substr(0, colon)));
        std::string value = util::trim(line.substr(colon + 1));
        response->headers[key] = value;
    } else if (line.rfind("HTTP/", 0) == 0) {
        // New status line: a redirect or continuation resets the headers.
        response->headers.clear();
        size_t space = line.find(' ');
        if (space != std::string::npos) {
            size_t reason_at = line.find(' ', space + 1);
            if (reason_at != std::string::npos) {
                response->reason = util::trim(line.substr(reason_at + 1));
            }
        }
    }
    return size * count;
}

int progress_thunk(void* clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* should_abort = static_cast<const std::function<bool()>*>(clientp);
    return (*should_abort && (*should_abort)()) ? 1 : 0;
}

}  // namespace

CurlTransport::CurlTransport(long timeout_seconds) : timeout_seconds_(timeout_seconds) {
    global_init_once();
    handle_ = curl_easy_init();
    if (handle_ == nullptr) throw TransportError("could not initialize libcurl");
}

CurlTransport::~CurlTransport() {
    if (handle_ != nullptr) curl_easy_cleanup(static_cast<CURL*>(handle_));
}

Response CurlTransport::send(const Request& request,
                             const std::function<bool()>& should_abort) {
    CURL* curl = static_cast<CURL*>(handle_);
    curl_easy_reset(curl);

    Response response;
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

    if (should_abort) {
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_thunk);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &should_abort);
    }

    if (request.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (request.method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());
    }
    if (!request.body.empty() || request.method == "POST" || request.method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(request.body.size()));
    }

    curl_slist* headers = nullptr;
    for (const auto& [key, value] : request.headers) {
        headers = curl_slist_append(headers, (key + ": " + value).c_str());
    }
    if (headers != nullptr) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode code = curl_easy_perform(curl);
    if (headers != nullptr) curl_slist_free_all(headers);
    if (code != CURLE_OK) {
        throw TransportError(curl_easy_strerror(code));
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    response.status = static_cast<int>(status);
    return response;
}

std::unique_ptr<HttpTransport> default_transport() {
    return std::make_unique<CurlTransport>();
}

}  // namespace lcpush::http
