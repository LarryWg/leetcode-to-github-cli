// Terminal rendering: colors, preview box, ready-to-push panel.
// The panel builders are pure string functions so tests can assert on them.
#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace lcpush::ui {

inline constexpr const char* kReset = "\033[0m";
inline constexpr const char* kDim = "\033[2m";
inline constexpr const char* kBold = "\033[1m";
inline constexpr const char* kRed = "\033[31m";
inline constexpr const char* kGreen = "\033[32m";
inline constexpr const char* kYellow = "\033[33m";
inline constexpr const char* kCyan = "\033[36m";

inline constexpr int kHeadLines = 5;
inline constexpr int kTailLines = 3;

// NO_COLOR disables, LCPUSH_FORCE_COLOR forces, otherwise stdout tty state.
bool color_enabled();

// Test seam: redirect where the print helpers write. nullptr restores the
// process stdout/stderr.
void set_streams(std::FILE* out, std::FILE* err);
std::FILE* out_stream();
std::FILE* err_stream();

std::string paint(const std::string& text, const char* code);

void success(const std::string& message);
void info(const std::string& message);
void dim(const std::string& message);
void warn(const std::string& message);
void error(const std::string& message);
void arrow(const std::string& message);

// First 5 and last 3 lines, with an explicit hidden-count marker between.
std::vector<std::string> preview_lines(const std::string& text);

// The bordered preview box, without the trailing confirm prompt.
std::string render_preview(const std::string& source_label, const std::string& text,
                           const std::string& language_label);

struct PushPanel {
    std::string filename;
    int lines = 0;
    std::string repo;
    std::string branch;
    std::string message;
    bool updating = false;
    std::string prompt_mode = "confirm";
};

// The ready-to-push panel, including its key hints.
std::string render_push_panel(const PushPanel& panel);

}  // namespace lcpush::ui
