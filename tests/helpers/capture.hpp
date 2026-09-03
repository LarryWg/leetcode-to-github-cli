// Captures everything the ui module prints, the capsys of the C++ suite.
#pragma once

#include <cstdio>
#include <string>

#include "lcpush/ui.hpp"

namespace lcpush::testing {

class CaptureStreams {
  public:
    CaptureStreams() : out_(std::tmpfile()), err_(std::tmpfile()) {
        ui::set_streams(out_, err_);
    }

    ~CaptureStreams() {
        ui::set_streams(nullptr, nullptr);
        if (out_ != nullptr) std::fclose(out_);
        if (err_ != nullptr) std::fclose(err_);
    }

    CaptureStreams(const CaptureStreams&) = delete;
    CaptureStreams& operator=(const CaptureStreams&) = delete;

    std::string out() const { return read_all(out_); }
    std::string err() const { return read_all(err_); }

  private:
    static std::string read_all(std::FILE* stream) {
        if (stream == nullptr) return "";
        long position = std::ftell(stream);
        std::rewind(stream);
        std::string content;
        char buffer[4096];
        size_t n = 0;
        while ((n = std::fread(buffer, 1, sizeof(buffer), stream)) > 0) {
            content.append(buffer, n);
        }
        std::fseek(stream, position, SEEK_SET);
        return content;
    }

    std::FILE* out_;
    std::FILE* err_;
};

}  // namespace lcpush::testing
