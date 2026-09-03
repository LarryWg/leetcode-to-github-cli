// Language detection by weighted regex signals. Detection is a ranking, not a
// verdict: callers show the full list with the winner preselected.
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lcpush::detect {

inline constexpr int kThreshold = 4;

struct Language {
    std::string key;
    std::string label;
    std::string ext;

    bool operator==(const Language&) const = default;
};

// All 14 supported languages in fixed declaration order.
const std::vector<Language>& languages();

// Lookup by key, nullptr when unknown.
const Language* by_key(std::string_view key);

// Resolve a user-supplied name through the alias table.
const Language* resolve_language(std::string_view name);

// Raw per-language scores after ambiguity gates.
std::map<std::string, int> score(const std::string& text);

// Every supported language, highest score first, stable within ties.
std::vector<std::pair<Language, int>> rank(const std::string& text);

struct Detection {
    std::optional<Language> language;
    int score = 0;
    std::vector<std::pair<Language, int>> ranked;

    bool confident() const { return language.has_value(); }
    std::string label() const { return language ? language->label : "unknown"; }
};

// Best-scoring language, or no language below threshold.
Detection detect(const std::string& text);

}  // namespace lcpush::detect
