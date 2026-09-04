// Target paths and commit-message rendering.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lcpush/domain/detect.hpp"
#include "lcpush/leetcode/problems.hpp"

namespace lcpush::render {

inline constexpr int kSubjectSoftLimit = 72;

// {path}{id:0>4}-{slug}{ext}, e.g. solutions/0001-two-sum.py
std::string target_path(const std::string& prefix, const Question& question,
                        const detect::Language& language);

std::string filename(const Question& question, const detect::Language& language);

std::map<std::string, std::string> message_vars(const Question& question,
                                                const detect::Language& language,
                                                int lines,
                                                const std::string& prefix = "");

struct RenderedMessage {
    std::string text;
    std::optional<std::string> warning;
};

// Render a template, degrading to fallback rather than ever crashing.
RenderedMessage render_template(const std::string& tmpl,
                                const std::map<std::string, std::string>& variables,
                                const std::string& fallback);

struct MessageSpec {
    int lines = 0;
    bool updating = false;
    std::string message_template;
    std::string update_template;
    std::string prefix;
};

// The template that applies, rendered. Never blank, never a crash.
RenderedMessage render_message(const Question& question, const detect::Language& language,
                               const MessageSpec& spec);

// Trim trailing whitespace per line, preserve the body of a multi-line message.
std::string clean_message(const std::string& message);

std::optional<std::string> subject_warning(const std::string& message);

// Variable names in the template that lcpush cannot fill.
std::vector<std::string> unknown_template_fields(
    const std::string& tmpl, const std::map<std::string, std::string>& variables);

}  // namespace lcpush::render
