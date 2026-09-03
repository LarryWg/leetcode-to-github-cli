#include "lcpush/term/terminal.hpp"

#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>

#include "lcpush/term/keys.hpp"

namespace lcpush::term {

namespace {

class StdinBytes final : public ByteSource {
  public:
    int next_byte() override {
        unsigned char byte = 0;
        while (true) {
            ssize_t n = ::read(STDIN_FILENO, &byte, 1);
            if (n == 1) return byte;
            if (n == 0) return -1;
            if (errno != EINTR) return -1;
        }
    }

    bool pending(int timeout_ms) override {
        struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};
        return ::poll(&pfd, 1, timeout_ms) > 0;
    }
};

class StdoutWriter final : public Writer {
  public:
    void write(std::string_view text) override {
        std::fwrite(text.data(), 1, text.size(), stdout);
        std::fflush(stdout);
    }
};

std::pair<int, int> terminal_size() {
    struct winsize size {};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return {size.ws_col, size.ws_row};
    }
    return {80, 24};
}

}  // namespace

Terminal& real_terminal() {
    static StdinBytes bytes;
    static KeyDecoder decoder(bytes);
    static StdoutWriter writer;
    static Terminal terminal{decoder, writer, terminal_size,
                             ::isatty(STDIN_FILENO) != 0};
    return terminal;
}

}  // namespace lcpush::term
