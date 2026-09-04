#include "lcpush/domain/render.hpp"

#include <cctype>
#include <regex>
#include <set>
#include <stdexcept>

#include "lcpush/config/config.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::render {

namespace {

// Raised internally when a template cannot be rendered. The message becomes
// the warning detail, matching what Python's Formatter errors produced.
struct TemplateError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

bool is_word(const std::string& text) {
    if (text.empty()) return false;
    for (char c : text) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }
    return true;
}

// Python repr for a string: single quotes unless the text contains one.
std::string repr(const std::string& text) {
    char quote = '\'';
    if (text.find('\'') != std::string::npos && text.find('"') == std::string::npos) {
        quote = '"';
    }
    std::string out(1, quote);
    for (char c : text) {
        if (c == '\\' || c == quote) {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            out.push_back(c);
        }
    }
    out.push_back(quote);
    return out;
}

// Apply a Python string format spec: [[fill]align][width][.precision][s].
std::string apply_spec(const std::string& value, const std::string& spec) {
    if (spec.empty()) return value;
    size_t pos = 0;
    char fill = ' ';
    char align = '<';
    if (spec.size() >= 2 && (spec[1] == '<' || spec[1] == '>' || spec[1] == '^')) {
        fill = spec[0];
        align = spec[1];
        pos = 2;
    } else if (spec[0] == '<' || spec[0] == '>' || spec[0] == '^') {
        align = spec[0];
        pos = 1;
    }
    size_t width = 0;
    while (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
        width = width * 10 + static_cast<size_t>(spec[pos] - '0');
        ++pos;
    }
    std::string text = value;
    if (pos < spec.size() && spec[pos] == '.') {
        ++pos;
        size_t precision = 0;
        bool any = false;
        while (pos < spec.size() && std::isdigit(static_cast<unsigned char>(spec[pos]))) {
            precision = precision * 10 + static_cast<size_t>(spec[pos] - '0');
            ++pos;
            any = true;
        }
        if (!any) throw TemplateError("Format specifier missing precision");
        text = util::codepoint_prefix(text, precision);
    }
    if (pos < spec.size() && spec[pos] == 's') ++pos;
    if (pos != spec.size()) {
        throw TemplateError("Invalid format specifier '" + spec + "' for object of type 'str'");
    }
    size_t length = util::codepoint_count(text);
    if (length >= width) return text;
    size_t pad = width - length;
    switch (align) {
        case '>': return std::string(pad, fill) + text;
        case '^': {
            size_t left = pad / 2;
            return std::string(left, fill) + text + std::string(pad - left, fill);
        }
        default: return text + std::string(pad, fill);
    }
}

// A close port of string.Formatter.vformat for named string variables.
std::string vformat(const std::string& tmpl,
                    const std::map<std::string, std::string>& variables) {
    std::string out;
    size_t i = 0;
    const size_t n = tmpl.size();
    while (i < n) {
        char c = tmpl[i];
        if (c == '{') {
            if (i + 1 < n && tmpl[i + 1] == '{') {
                out.push_back('{');
                i += 2;
                continue;
            }
            size_t close = tmpl.find('}', i + 1);
            if (close == std::string::npos) {
                throw TemplateError("Single '{' encountered in format string");
            }
            std::string field = tmpl.substr(i + 1, close - i - 1);
            std::string spec;
            std::string conversion;
            size_t colon = field.find(':');
            if (colon != std::string::npos) {
                spec = field.substr(colon + 1);
                field = field.substr(0, colon);
            }
            size_t bang = field.find('!');
            if (bang != std::string::npos) {
                conversion = field.substr(bang + 1);
                field = field.substr(0, bang);
            }
            if (field.empty() || util::is_all_digits(field)) {
                throw TemplateError("Replacement index out of range");
            }
            if (!is_word(field)) {
                throw TemplateError(field);
            }
            auto hit = variables.find(field);
            if (hit == variables.end()) {
                throw TemplateError(field);
            }
            std::string value = hit->second;
            if (conversion == "r") {
                value = repr(value);
            } else if (!conversion.empty() && conversion != "s") {
                throw TemplateError("Unknown conversion specifier " + conversion);
            }
            out += apply_spec(value, spec);
            i = close + 1;
            continue;
        }
        if (c == '}') {
            if (i + 1 < n && tmpl[i + 1] == '}') {
                out.push_back('}');
                i += 2;
                continue;
            }
            throw TemplateError("Single '}' encountered in format string");
        }
        out.push_back(c);
        i += 1;
    }
    return out;
}

}  // namespace

std::string target_path(const std::string& prefix, const Question& question,
                        const detect::Language& language) {
    return prefix + question.padded_id() + "-" + question.slug + language.ext;
}

std::string filename(const Question& question, const detect::Language& language) {
    return question.padded_id() + "-" + question.slug + language.ext;
}

std::map<std::string, std::string> message_vars(const Question& question,
                                                const detect::Language& language,
                                                int lines,
                                                const std::string& prefix) {
    std::string name = filename(question, language);
    return {
        {"id", question.id},
        {"padded_id", question.padded_id()},
        {"title", question.title},
        {"slug", question.slug},
        {"language", language.label},
        {"ext", language.ext},
        {"difficulty", question.difficulty},
        {"filename", name},
        {"path", prefix + name},
        {"lines", std::to_string(lines)},
    };
}

RenderedMessage render_template(const std::string& tmpl,
                                const std::map<std::string, std::string>& variables,
                                const std::string& fallback) {
    try {
        return RenderedMessage{util::trim(vformat(tmpl, variables)), std::nullopt};
    } catch (const TemplateError& exc) {
        std::string detail = util::strip_chars(exc.what(), "'\"");
        if (detail.empty()) detail = "ValueError";
        std::string warning =
            "⚠ Commit template is invalid (" + detail + "); using the built-in default.";
        return RenderedMessage{util::trim(vformat(fallback, variables)), warning};
    }
}

RenderedMessage render_message(const Question& question, const detect::Language& language,
                               const MessageSpec& spec) {
    auto variables = message_vars(question, language, spec.lines, spec.prefix);
    const std::string& tmpl =
        spec.updating ? spec.update_template : spec.message_template;
    const std::string fallback = spec.updating ? config::kDefaultUpdateTemplate
                                               : config::kDefaultMessageTemplate;
    RenderedMessage rendered = render_template(tmpl, variables, fallback);
    if (util::trim(rendered.text).empty()) {
        return RenderedMessage{
            util::trim(vformat(fallback, variables)),
            "⚠ Commit template rendered empty; using the built-in default."};
    }
    return rendered;
}

std::string clean_message(const std::string& message) {
    std::string unified = util::replace_all(message, "\r\n", "\n");
    std::vector<std::string> lines;
    for (const std::string& line : util::split(unified, '\n')) {
        lines.push_back(util::rstrip(line));
    }
    return util::trim(util::join(lines, "\n"));
}

std::optional<std::string> subject_warning(const std::string& message) {
    std::string subject = util::split(message, '\n')[0];
    size_t length = util::codepoint_count(subject);
    if (length > static_cast<size_t>(kSubjectSoftLimit)) {
        return "⚠ Commit subject is " + std::to_string(length) + " characters (over " +
               std::to_string(kSubjectSoftLimit) + "); accepted anyway.";
    }
    return std::nullopt;
}

std::vector<std::string> unknown_template_fields(
    const std::string& tmpl, const std::map<std::string, std::string>& variables) {
    static const std::regex field_rx(R"(\{(\w+))");
    std::set<std::string> unknown;
    for (auto it = std::sregex_iterator(tmpl.begin(), tmpl.end(), field_rx);
         it != std::sregex_iterator(); ++it) {
        std::string name = (*it)[1].str();
        if (variables.find(name) == variables.end()) unknown.insert(name);
    }
    return {unknown.begin(), unknown.end()};
}

}  // namespace lcpush::render
