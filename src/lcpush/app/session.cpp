#include "lcpush/app/session.hpp"

#include "lcpush/config/config.hpp"
#include "lcpush/domain/detect.hpp"
#include "lcpush/core/errors.hpp"
#include "lcpush/app/onboarding.hpp"
#include "lcpush/domain/plausibility.hpp"
#include "lcpush/domain/render.hpp"
#include "lcpush/domain/solution.hpp"
#include "lcpush/github/tokens.hpp"
#include "lcpush/ui/console.hpp"
#include "lcpush/util/strings.hpp"

namespace lcpush::session {

namespace {

struct SolutionInput {
    std::string text;
    std::string source_label;
    detect::Detection detection;
};

// -- configuration ---------------------------------------------------------

std::pair<config::Config, std::string> load_config(const flow::Deps& deps) {
    auto config = config::load();
    if (!config || !config->repo.configured()) {
        return onboarding::setup(deps, config);
    }
    std::string token = onboarding::resolve_token(deps, true);
    return {*config, token};
}

// -- question selection ----------------------------------------------------

Question choose_question(const flow::Deps& deps, const config::Config& config,
                         bool refresh, problems::RefreshHandle* refresh_out) {
    problems::GetQuestionsOptions options;
    options.ttl_days = config.cache.problems_ttl_days;
    options.refresh = refresh;
    options.transport_factory = deps.problems_transport;
    options.warn = [](const std::string& message) { ui::dim("  " + message); };
    options.info = [](const std::string& message) { ui::dim("  " + message); };
    auto questions = problems::get_questions(options, refresh_out);
    Question question = deps.pick(search::build_index(questions));
    ui::success(question.display());
    return question;
}

// -- solution input --------------------------------------------------------

// Menu entries plus the preselected value, ordered by plausibility.
std::pair<std::vector<prompts::Choice>, std::string> source_choices(const flow::Deps& deps) {
    std::vector<prompts::Choice> entries;
    auto clip_text = deps.clipboard_read();
    std::string default_value = "editor";

    if (clip_text.has_value()) {
        if (plausibility::assess(clip_text).plausible) {
            entries.push_back({"Clipboard", "clipboard"});
            default_value = "clipboard";
        } else {
            entries.push_back({"Clipboard (doesn't look like code)", "clipboard"});
        }
    }
    entries.push_back({"Editor", "editor"});
    entries.push_back({"Stdin", "stdin"});
    return {entries, default_value};
}

// (text, label) for a chosen source. Throws InputError when unusable.
std::pair<std::string, std::string> read_source(const flow::Deps& deps,
                                                const std::string& source) {
    if (source == "clipboard") {
        auto text = deps.clipboard_read();
        if (!text) throw InputError("Clipboard is empty.");
        return {*text, "Clipboard"};
    }
    if (source == "editor") {
        auto text = deps.open_editor({});
        if (!text) throw InputError("No solution provided. Aborted.");
        return {*text, "Editor"};
    }
    ui::dim("  Paste your solution, then Ctrl-D (or a lone EOF line) to finish.");
    return {deps.read_stdin(), "Stdin"};
}

// Read, validate, preview and confirm a solution. Declining the preview
// returns to the source menu rather than exiting.
SolutionInput read_solution(const flow::Deps& deps, const Question& question) {
    while (true) {
        auto [entries, default_value] = source_choices(deps);
        std::string source = deps.select("? Solution source:", entries, default_value);
        std::string raw;
        std::string label;
        try {
            std::tie(raw, label) = read_source(deps, source);
        } catch (const InputError& exc) {
            // A clipboard that emptied since the menu was drawn is worth a
            // retry; an empty editor buffer is an explicit abort.
            if (source != "clipboard") throw;
            ui::warn(std::string("⚠ ") + exc.what());
            continue;
        }

        auto problem = solution::reject_reason(raw);
        if (problem) {
            ui::warn("⚠ " + *problem);
            continue;
        }

        std::string text = solution::normalize(raw);
        detect::Detection detection = detect::detect(text);
        ui::success("Read " + std::to_string(solution::line_count(text)) + " lines from " +
                    util::to_lower(label));

        auto warnings =
            solution::soft_warnings(text, &detection, question.slug, question.title);
        ui::info("");
        ui::info(ui::render_preview(label, text, detection.label()));
        for (const std::string& message : warnings) ui::warn(message);
        if (deps.confirm("  └ Use this?", true)) {
            return SolutionInput{text, label, detection};
        }
        ui::info("");
    }
}

// -- language --------------------------------------------------------------

detect::Language choose_language(const flow::Deps& deps, const SolutionInput& solution) {
    const detect::Detection& detection = solution.detection;
    std::vector<prompts::Choice> entries;
    for (const auto& [language, score] : detection.ranked) {
        bool is_detected = detection.language && language.key == detection.language->key;
        entries.push_back(
            {is_detected ? language.label + "  (detected)" : language.label, language.key});
    }
    std::optional<std::string> default_value;
    if (detection.language) {
        default_value = detection.language->key;
    } else {
        ui::warn("⚠ Could not identify a programming language — choose one.");
    }
    std::string chosen = deps.select("? Language:", entries, default_value);
    const detect::Language* language = detect::resolve_language(chosen);
    return language != nullptr ? *language : detection.ranked[0].first;
}

// -- commit message and push ----------------------------------------------

std::string edit_message(const flow::Deps& deps, std::string current, bool in_editor);

std::string build_message(const flow::Deps& deps, const Question& question,
                          const detect::Language& language, const config::Config& config,
                          int lines, bool updating) {
    render::RenderedMessage rendered =
        render::render_message(question, language,
                               {.lines = lines,
                                .updating = updating,
                                .message_template = config.commit.message_template,
                                .update_template = config.commit.update_template,
                                .prefix = config.repo.path});
    if (rendered.warning) ui::warn(*rendered.warning);
    std::string message = rendered.text;
    if (config.commit.prompt == "always") {
        message = edit_message(deps, message, false);
    }
    return message;
}

// Re-prompt until non-empty; never silently fall through to a default.
std::string edit_message(const flow::Deps& deps, std::string current, bool in_editor) {
    while (true) {
        std::string candidate;
        if (in_editor) {
            auto edited = deps.open_editor(
                {.initial = current,
                 .header = "# Edit the commit message. Lines starting with # are kept.\n",
                 .suffix = ".txt"});
            candidate = render::clean_message(edited.value_or(""));
        } else {
            ui::dim("    Enter to confirm, Ctrl-U to clear");
            candidate =
                render::clean_message(deps.edit_line("? Commit message:  ", current));
        }
        if (!candidate.empty()) {
            auto warning = render::subject_warning(candidate);
            if (warning) ui::warn(*warning);
            return candidate;
        }
        ui::warn("⚠ Commit message cannot be empty.");
    }
}

struct PushPrompt {
    std::string path;
    int lines = 0;
    std::string repo;
    std::string branch;
    std::string message;
    bool updating = false;
    std::string prompt_mode;
};

// Draw the ready-to-push panel until the user pushes or cancels.
std::string confirm_push(const flow::Deps& deps, const PushPrompt& prompt) {
    std::string current = prompt.message;
    bool editable = prompt.prompt_mode != "never";
    while (true) {
        ui::info("");
        ui::info(ui::render_push_panel({.filename = prompt.path,
                                        .lines = prompt.lines,
                                        .repo = prompt.repo,
                                        .branch = prompt.branch,
                                        .message = current,
                                        .updating = prompt.updating,
                                        .prompt_mode = prompt.prompt_mode}));
        std::map<std::string, std::string> allowed = {
            {"enter", "push"}, {"n", "cancel"}, {"escape", "cancel"}};
        if (editable) {
            allowed["m"] = "edit";
            allowed["M"] = "editor";
        }
        std::string action = deps.read_key(allowed);
        if (action == "push") return current;
        if (action == "cancel") throw Cancelled("Cancelled — nothing was pushed.");
        current = edit_message(deps, current, action == "editor");
    }
}

// Resolve add-vs-overwrite, confirm, and PUT the file. Returns the URL.
std::string push(const flow::Deps& deps, const config::Config& config,
                 const std::string& token, const Question& question,
                 const detect::Language& language, const SolutionInput& solution) {
    std::string path = render::target_path(config.repo.path, question, language);
    std::string name = render::filename(question, language);
    int lines = solution::line_count(solution.text);

    auto github = deps.github(token);
    auto sha = github->get_file_sha(config.repo.owner, config.repo.name, path,
                                    config.repo.branch);
    bool updating = sha.has_value();

    std::string message = build_message(deps, question, language, config, lines, updating);
    message = confirm_push(deps, {.path = name,
                                  .lines = lines,
                                  .repo = config.repo.full_name(),
                                  .branch = config.repo.branch,
                                  .message = message,
                                  .updating = updating,
                                  .prompt_mode = config.commit.prompt});

    ui::info("");
    ui::arrow("Pushing " + path + " to " + config.repo.full_name() + " (" +
              config.repo.branch + ")");
    github::PushResult result =
        github->put_file(config.repo.owner, config.repo.name, path,
                         {.content = solution.text,
                          .message = message,
                          .branch = config.repo.branch,
                          .sha = sha,
                          .author_name = config.commit.author_name,
                          .author_email = config.commit.author_email});
    return result.html_url;
}

}  // namespace

int run(const flow::Deps& deps, bool refresh) {
    if (!deps.stdin_isatty()) {
        throw InputError("lcpush is interactive and needs a terminal.");
    }

    auto [config, token] = load_config(deps);
    // Held for the whole session so a stale-cache refresh can finish.
    problems::RefreshHandle background_refresh;
    Question question = choose_question(deps, config, refresh, &background_refresh);
    SolutionInput solution = read_solution(deps, question);
    detect::Language language = choose_language(deps, solution);
    std::string url = push(deps, config, token, question, language, solution);
    ui::success(tokens::redact(url, token));
    return 0;
}

}  // namespace lcpush::session
