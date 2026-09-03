#include "lcpush/term/raw_mode.hpp"

#include <unistd.h>

namespace lcpush::term {

RawMode::RawMode(bool enabled) {
    if (!enabled || ::isatty(STDIN_FILENO) == 0) return;
    if (::tcgetattr(STDIN_FILENO, &saved_) != 0) return;
    struct termios raw = saved_;
    // ISIG off: Ctrl-C arrives as byte 0x03 and becomes Cancelled in exactly
    // one place. Output processing stays on so \n still renders as CRLF.
    raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_iflag &= ~static_cast<tcflag_t>(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return;
    active_ = true;
}

RawMode::~RawMode() {
    if (active_) ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_);
}

}  // namespace lcpush::term
