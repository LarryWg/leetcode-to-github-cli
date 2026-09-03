// Inline frame renderer: repaint a block of lines in place below the shell
// prompt, then clear it and leave a one-line residue when the widget is done.
#pragma once

#include <string>

#include "lcpush/term/terminal.hpp"

namespace lcpush::term {

class Frame {
  public:
    explicit Frame(Writer& out) : out_(out) {}

    // Repaint the block. cursor_row/cursor_col (0-based row, 0-based column
    // in code points) position the terminal cursor inside the block; a
    // negative row parks the cursor after the last line.
    void render(const std::string& content, int cursor_row = -1, int cursor_col = 0);

    // Erase the block entirely.
    void clear();

    // Erase the block and print residue as ordinary scrollback output.
    void finish(const std::string& residue);

  private:
    void move_to_top();

    Writer& out_;
    int lines_ = 0;       // lines the current block occupies
    int cursor_row_ = 0;  // row the terminal cursor was left on
};

}  // namespace lcpush::term
