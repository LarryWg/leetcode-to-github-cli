#include "lcpush/term/keys.hpp"

namespace lcpush::term {

namespace {

// Continuation byte count for a UTF-8 lead byte.
int utf8_extra(unsigned char lead) {
    if ((lead >> 5) == 0x6) return 1;
    if ((lead >> 4) == 0xe) return 2;
    if ((lead >> 3) == 0x1e) return 3;
    return 0;
}

}  // namespace

Key KeyDecoder::next() {
    int byte = bytes_.next_byte();
    if (byte < 0) return Key::of(Key::Type::Eof);
    unsigned char c = static_cast<unsigned char>(byte);

    switch (c) {
        case 0x01: return Key::of(Key::Type::CtrlA);
        case 0x03: return Key::of(Key::Type::CtrlC);
        case 0x04: return Key::of(Key::Type::CtrlD);
        case 0x05: return Key::of(Key::Type::CtrlE);
        case 0x0e: return Key::of(Key::Type::CtrlN);
        case 0x10: return Key::of(Key::Type::CtrlP);
        case 0x15: return Key::of(Key::Type::CtrlU);
        case '\r':
        case '\n': return Key::of(Key::Type::Enter);
        case 0x7f:
        case 0x08: return Key::of(Key::Type::Backspace);
        case 0x1b: return decode_escape();
        default: break;
    }

    if (c < 0x20) {
        // Other control bytes carry no meaning here; skip to the next key.
        return next();
    }

    std::string text(1, static_cast<char>(c));
    if (c >= 0x80) {
        for (int i = 0; i < utf8_extra(c); ++i) {
            int cont = bytes_.next_byte();
            if (cont < 0) break;
            text.push_back(static_cast<char>(cont));
        }
    }
    return Key::ch(std::move(text));
}

Key KeyDecoder::decode_escape() {
    // No byte hot on the heels of ESC means the user pressed Esc itself.
    if (!bytes_.pending(kEscTimeoutMs)) return Key::of(Key::Type::Esc);
    int second = bytes_.next_byte();
    if (second < 0) return Key::of(Key::Type::Esc);

    if (second == '[' || second == 'O') {
        // CSI (or SS3): parameters then a final byte in 0x40..0x7e.
        std::string params;
        while (true) {
            int byte = bytes_.next_byte();
            if (byte < 0) return Key::of(Key::Type::Esc);
            char c = static_cast<char>(byte);
            if (byte >= 0x40 && byte <= 0x7e) {
                switch (c) {
                    case 'A': return Key::of(Key::Type::Up);
                    case 'B': return Key::of(Key::Type::Down);
                    case 'C': return Key::of(Key::Type::Right);
                    case 'D': return Key::of(Key::Type::Left);
                    case 'H': return Key::of(Key::Type::Home);
                    case 'F': return Key::of(Key::Type::End);
                    case '~':
                        if (params == "1" || params == "7") return Key::of(Key::Type::Home);
                        if (params == "4" || params == "8") return Key::of(Key::Type::End);
                        return next();  // unknown sequence: swallow, read on
                    default:
                        return next();
                }
            }
            params.push_back(c);
        }
    }
    // Alt-<key> and other escape pairs are not bound: swallow the pair.
    return next();
}

}  // namespace lcpush::term
