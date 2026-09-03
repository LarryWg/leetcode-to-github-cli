// Scripted HttpTransport, the counterpart of httpx.MockTransport.
#pragma once

#include <functional>
#include <memory>

#include "lcpush/http/transport.hpp"

namespace lcpush::testing {

class FakeTransport final : public http::HttpTransport {
  public:
    using Handler = std::function<http::Response(const http::Request&)>;

    explicit FakeTransport(Handler handler) : handler_(std::move(handler)) {}

    http::Response send(const http::Request& request,
                        const std::function<bool()>&) override {
        return handler_(request);
    }

  private:
    Handler handler_;
};

inline std::shared_ptr<FakeTransport> make_transport(FakeTransport::Handler handler) {
    return std::make_shared<FakeTransport>(std::move(handler));
}

inline http::Response json_response(int status, const std::string& body) {
    http::Response response;
    response.status = status;
    response.body = body;
    return response;
}

}  // namespace lcpush::testing
