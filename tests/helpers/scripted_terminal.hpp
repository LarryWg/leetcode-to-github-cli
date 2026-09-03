// Scripted terminal: feed raw key bytes (like prompt_toolkit's pipe input)
// and capture everything the widget writes.
#pragma once

#include <deque>
#include <stdexcept>
#include <string>

#include "lcpush/term/keys.hpp"
#include "lcpush/term/terminal.hpp"

namespace lcpush::testing {

class ScriptedBytes final : public term::ByteSource {
  public:
    explicit ScriptedBytes(const std::string& bytes)
        : bytes_(bytes.begin(), bytes.end()) {}

    int next_byte() override {
        if (bytes_.empty()) return -1;
        int byte = static_cast<unsigned char>(bytes_.front());
        bytes_.pop_front();
        return byte;
    }

    bool pending(int) override { return !bytes_.empty(); }

  private:
    std::deque<char> bytes_;
};

class StringWriter final : public term::Writer {
  public:
    void write(std::string_view text) override { captured += text; }

    std::string captured;
};

// Owns the pieces of a scripted Terminal and hands out the view struct.
class ScriptedTerminal {
  public:
    explicit ScriptedTerminal(const std::string& bytes, int cols = 80, int rows = 24)
        : bytes_(bytes), decoder_(bytes_),
          terminal_{decoder_, writer_, [cols, rows] { return std::pair{cols, rows}; },
                    false} {}

    term::Terminal& terminal() { return terminal_; }
    const std::string& output() const { return writer_.captured; }

  private:
    ScriptedBytes bytes_;
    term::KeyDecoder decoder_;
    StringWriter writer_;
    term::Terminal terminal_;
};

}  // namespace lcpush::testing
