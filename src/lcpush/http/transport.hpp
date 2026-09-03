// HTTP seam. Production is CurlTransport, tests install a scripted transport
// the way the Python tests used httpx.MockTransport.
#pragma once

#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lcpush::http {

struct Request {
    std::string method;
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

struct Response {
    int status = 0;
    std::string body;
    // Header names lowercased.
    std::map<std::string, std::string> headers;
    std::string reason;
};

// Network-level failure (DNS, refused, timeout). Callers wrap this into the
// product-specific single-line error.
struct TransportError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class HttpTransport {
  public:
    virtual ~HttpTransport() = default;

    // Perform the request, or throw TransportError. An abort check may be
    // polled during the transfer so a stop request cancels promptly.
    virtual Response send(const Request& request,
                          const std::function<bool()>& should_abort = nullptr) = 0;
};

using TransportFactory = std::function<std::unique_ptr<HttpTransport>()>;

}  // namespace lcpush::http
