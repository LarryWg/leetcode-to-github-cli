#include "lcpush/term/render.hpp"

#include <vector>

#include "lcpush/util/strings.hpp"

namespace lcpush::term {

void Frame::move_to_top() {
    out_.write("\r");
    if (cursor_row_ > 0) {
        out_.write("\x1b[" + std::to_string(cursor_row_) + "A");
    }
}

void Frame::render(const std::string& content, int cursor_row, int cursor_col) {
    std::vector<std::string> lines = util::split(content, '\n');
    move_to_top();
    out_.write("\x1b[?25l\x1b[0J");
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) out_.write("\n");
        out_.write(lines[i]);
    }
    lines_ = static_cast<int>(lines.size());
    cursor_row_ = lines_ - 1;

    if (cursor_row >= 0 && cursor_row < lines_) {
        int up = cursor_row_ - cursor_row;
        if (up > 0) out_.write("\x1b[" + std::to_string(up) + "A");
        out_.write("\x1b[" + std::to_string(cursor_col + 1) + "G");
        cursor_row_ = cursor_row;
    }
    out_.write("\x1b[?25h");
}

void Frame::clear() {
    move_to_top();
    out_.write("\x1b[0J");
    lines_ = 0;
    cursor_row_ = 0;
}

void Frame::finish(const std::string& residue) {
    clear();
    if (!residue.empty()) {
        out_.write(residue);
        out_.write("\n");
    }
}

}  // namespace lcpush::term
