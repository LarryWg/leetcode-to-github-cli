// The terminal seam. Widgets read decoded keys from a KeySource and write
// ANSI to a Writer, so tests drive them with scripted bytes and a string
// capture instead of a pty.
#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace lcpush::term {

struct Key {
    enum class Type {
        Char,       // printable text, one code point in `text`
        Enter,
        Backspace,
        Up,
        Down,
        Left,
        Right,
        Home,
        End,
        Esc,
        CtrlA,
        CtrlC,
        CtrlD,
        CtrlE,
        CtrlN,
        CtrlP,
        CtrlU,
        Eof,
    };

    Type type = Type::Eof;
    std::string text;

    static Key of(Type type) { return Key{type, ""}; }
    static Key ch(std::string text) { return Key{Type::Char, std::move(text)}; }

    bool operator==(const Key&) const = default;
};

// Blocking byte stream with a short-timeout peek, the decoder's input.
class ByteSource {
  public:
    virtual ~ByteSource() = default;

    // Next raw byte, or -1 on end of input.
    virtual int next_byte() = 0;

    // Whether a byte is available within timeout_ms. Drives the bare-Esc
    // versus escape-sequence disambiguation.
    virtual bool pending(int timeout_ms) = 0;
};

class KeySource {
  public:
    virtual ~KeySource() = default;
    virtual Key next() = 0;
};

class Writer {
  public:
    virtual ~Writer() = default;
    virtual void write(std::string_view text) = 0;
};

struct Terminal {
    KeySource& keys;
    Writer& out;
    // (columns, rows)
    std::function<std::pair<int, int>()> size;
    // True on a real tty: widgets enter raw mode and paint colors.
    bool interactive = false;
};

// The process terminal: stdin bytes, stdout writes, ioctl size.
Terminal& real_terminal();

}  // namespace lcpush::term
