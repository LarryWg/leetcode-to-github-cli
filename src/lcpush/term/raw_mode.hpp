// RAII raw mode for the controlling terminal. Restores the saved settings on
// destruction, so cancellation exceptions can never leave the shell broken.
#pragma once

#include <termios.h>

namespace lcpush::term {

class RawMode {
  public:
    // Enters raw mode on stdin when enabled and stdin is a tty.
    explicit RawMode(bool enabled);
    ~RawMode();

    RawMode(const RawMode&) = delete;
    RawMode& operator=(const RawMode&) = delete;

  private:
    bool active_ = false;
    struct termios saved_ {};
};

}  // namespace lcpush::term
