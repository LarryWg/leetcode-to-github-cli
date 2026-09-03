// Byte-stream key decoder: control bytes, CSI escape sequences with bare-Esc
// disambiguation, and UTF-8 characters. Pure over a ByteSource so it is unit
// testable without a terminal.
#pragma once

#include "lcpush/term/terminal.hpp"

namespace lcpush::term {

inline constexpr int kEscTimeoutMs = 50;

class KeyDecoder final : public KeySource {
  public:
    explicit KeyDecoder(ByteSource& bytes) : bytes_(bytes) {}

    Key next() override;

  private:
    Key decode_escape();

    ByteSource& bytes_;
};

}  // namespace lcpush::term
