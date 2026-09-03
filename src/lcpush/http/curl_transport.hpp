// libcurl implementation of the HttpTransport seam.
#pragma once

#include <memory>

#include "lcpush/http/transport.hpp"

namespace lcpush::http {

class CurlTransport final : public HttpTransport {
  public:
    // Timeout matches the 20s the Python httpx clients used.
    explicit CurlTransport(long timeout_seconds = 20);
    ~CurlTransport() override;

    CurlTransport(const CurlTransport&) = delete;
    CurlTransport& operator=(const CurlTransport&) = delete;

    Response send(const Request& request,
                  const std::function<bool()>& should_abort = nullptr) override;

  private:
    void* handle_ = nullptr;  // CURL*
    long timeout_seconds_;
};

std::unique_ptr<HttpTransport> default_transport();

}  // namespace lcpush::http
