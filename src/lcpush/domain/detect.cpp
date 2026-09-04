#include "lcpush/domain/detect.hpp"

#include <algorithm>
#include <regex>
#include <unordered_map>

#include "lcpush/util/strings.hpp"

namespace lcpush::detect {

namespace {

std::regex rx(const char* pattern) {
    return std::regex(pattern, std::regex::ECMAScript | std::regex::multiline);
}

struct Signal {
    std::regex pattern;
    int weight;
};

using SignalTable = std::vector<std::pair<std::string, std::vector<Signal>>>;

// (pattern, weight) per language, transliterated from the Python signal table.
const SignalTable& signals() {
    static const SignalTable table = {
        {"cpp",
         {
             {rx(R"(#include\s*<)"), 2},
             {rx(R"(std::)"), 3},
             {rx(R"(\bvector<)"), 3},
             {rx(R"(^\s*public:)"), 3},
             {rx(R"(\bnullptr\b)"), 2},
             {rx(R"(\bclass\s+Solution\s*\{)"), 1},
         }},
        {"c",
         {
             {rx(R"(#include\s*<(stdlib|stdio|string)\.h>)"), 3},
             {rx(R"(\bmalloc\s*\()"), 3},
             {rx(R"(\bint\s*\*)"), 2},
             {rx(R"(\breturnSize\b)"), 3},
             {rx(R"(\bcalloc\s*\(|\bfree\s*\()"), 2},
         }},
        {"python3",
         {
             {rx(R"(^\s*class\s+Solution\s*(\(object\))?\s*:)"), 4},
             {rx(R"(^\s*def\s+\w+\s*\(\s*self)"), 4},
             {rx(R"(\b(List|Dict|Optional|Tuple)\[)"), 2},
             {rx(R"(^\s*def\s+\w+\s*\(.*\)\s*->)"), 2},
             {rx(R"(^\s*(from|import)\s+\w+)"), 1},
         }},
        {"java",
         {
             {rx(R"(^\s*import\s+java\.)"), 4},
             {rx(R"(\bpublic\s+class\s+Solution\b)"), 2},
             {rx(R"(\bint\[\]\s+\w+)"), 2},
             {rx(R"(\bnew\s+(ArrayList|HashMap|HashSet)<)"), 3},
             {rx(R"(\bSystem\.out\.)"), 2},
         }},
        {"csharp",
         {
             {rx(R"(^\s*using\s+System)"), 4},
             {rx(R"(\bIList<|\bIDictionary<)"), 4},
             {rx(R"(\bpublic\s+class\s+Solution\b)"), 1},
             {rx(R"(\bpublic\s+[\w\[\]<>,\s]+\s+[A-Z]\w*\s*\()"), 2},
             {rx(R"(\bvar\s+\w+\s*=\s*new\s+\w)"), 1},
         }},
        {"javascript",
         {
             {rx(R"(@param\s*\{)"), 3},
             {rx(R"(\b(var|const|let)\s+\w+\s*=\s*function\s*\()"), 4},
             {rx(R"(\bmodule\.exports\b)"), 2},
             {rx(R"(\bfunction\s*\w*\s*\([^)]*\)\s*\{)"), 1},
             {rx(R"(===|!==)"), 1},
         }},
        {"typescript",
         {
             {rx(R"(function\s+\w+\s*\([^)]*\w+\s*:\s*\w)"), 4},
             {rx(R"(\)\s*:\s*(number|string|boolean|void|any)(\[\])?\s*(\{|=>))"), 4},
             {rx(R"(:\s*(number|string|boolean)\[\])"), 3},
             {rx(R"(^\s*(interface|type)\s+\w+)"), 2},
         }},
        {"golang",
         {
             {rx(R"(^\s*package\s+\w+)"), 4},
             {rx(R"(:=)"), 3},
             {rx(R"(^\s*func\s+\w+\s*\()"), 2},
             {rx(R"(\[\]int\b|\[\]string\b)"), 3},
             {rx(R"(^\s*import\s*\()"), 1},
         }},
        {"rust",
         {
             {rx(R"(\bimpl\s+Solution\b)"), 4},
             {rx(R"(\bpub\s+fn\b)"), 4},
             {rx(R"(\bVec<)"), 3},
             {rx(R"(&self\b|&mut\b)"), 2},
             {rx(R"(\bi32\b|\bi64\b|\busize\b)"), 2},
         }},
        {"ruby",
         {
             {rx(R"(@param\s*\{\w+)"), 2},
             {rx(R"(^\s*def\s+\w+.*$)"), 2},
             {rx(R"(^\s*end\s*$)"), 3},
             {rx(R"(\bnil\b)"), 2},
             {rx(R"(\.each\s+do\s*\||\.each_with_index)"), 3},
             {rx(R"(^\s*#\s*@(param|return))"), 2},
         }},
        {"swift",
         {
             {rx(R"(\bfunc\s+\w+\s*\(.*\)\s*->\s*\[)"), 4},
             {rx(R"(\bclass\s+Solution\s*\{)"), 1},
             {rx(R"(^\s*(var|let)\s+\w+\s*(=|:))"), 2},
             {rx(R"(\[Int\]|\[String\])"), 3},
             {rx(R"(\bguard\s+let\b|\bif\s+let\b)"), 2},
         }},
        {"kotlin",
         {
             {rx(R"(\bfun\s+\w+\s*\()"), 4},
             {rx(R"(\bIntArray\b|\bMutableList<)"), 4},
             {rx(R"(^\s*val\s+\w+)"), 2},
             {rx(R"(\bclass\s+Solution\s*\{)"), 1},
         }},
        {"scala",
         {
             {rx(R"(\bobject\s+Solution\b)"), 4},
             {rx(R"(\bdef\s+\w+\s*\(.*\)\s*:\s*Array\[)"), 4},
             {rx(R"(:\s*Array\[|\bList\[Int\])"), 3},
             {rx(R"(\bval\s+\w+\s*=)"), 1},
         }},
        {"php",
         {
             {rx(R"(<\?php)"), 4},
             {rx(R"(\bfunction\s+\w+\s*\([^)]*\$)"), 4},
             {rx(R"(\$\w+\s*=)"), 2},
             {rx(R"(->\w+\(|\barray\s*\()"), 1},
         }},
    };
    return table;
}

// Ambiguity gates: resolve C/C++, Java/C#, and JS/TS with hard rules.
const std::regex& has_class() {
    static const std::regex value = rx(R"(\bclass\b)");
    return value;
}
const std::regex& has_std() {
    static const std::regex value = rx(R"(std::|#include\s*<(iostream|vector|string|unordered_map)>)");
    return value;
}
const std::regex& csharp_force() {
    static const std::regex value = rx(R"(^\s*using\s+System|\bIList<)");
    return value;
}
const std::regex& ts_annotation() {
    static const std::regex value =
        rx(R"(\(\s*\w+\s*:\s*\w|,\s*\w+\s*:\s*\w+[\[\]]*\s*[,)]|\)\s*:\s*\w+(\[\])?\s*(\{|=>))");
    return value;
}

bool found(const std::string& text, const std::regex& pattern) {
    return std::regex_search(text, pattern);
}

void apply_gates(const std::string& text, std::map<std::string, int>& scores) {
    if (found(text, has_class()) || found(text, has_std())) {
        scores["c"] = 0;
    }
    if (found(text, csharp_force())) {
        scores["java"] = std::max(0, scores["java"] - 4);
    }
    if (found(text, ts_annotation()) && scores["typescript"] > 0) {
        scores["javascript"] = std::max(0, scores["javascript"] - 3);
    } else {
        scores["typescript"] = std::max(0, scores["typescript"] - 2);
    }
}

}  // namespace

const std::vector<Language>& languages() {
    static const std::vector<Language> value = {
        {"cpp", "C++", ".cpp"},       {"c", "C", ".c"},
        {"python3", "Python3", ".py"}, {"java", "Java", ".java"},
        {"csharp", "C#", ".cs"},      {"javascript", "JavaScript", ".js"},
        {"typescript", "TypeScript", ".ts"}, {"golang", "Go", ".go"},
        {"rust", "Rust", ".rs"},      {"ruby", "Ruby", ".rb"},
        {"swift", "Swift", ".swift"}, {"kotlin", "Kotlin", ".kt"},
        {"scala", "Scala", ".scala"}, {"php", "PHP", ".php"},
    };
    return value;
}

const Language* by_key(std::string_view key) {
    for (const Language& lang : languages()) {
        if (lang.key == key) return &lang;
    }
    return nullptr;
}

const Language* resolve_language(std::string_view name) {
    static const std::unordered_map<std::string, std::string> aliases = {
        {"c++", "cpp"},    {"cc", "cpp"},      {"cxx", "cpp"},
        {"py", "python3"}, {"python", "python3"}, {"py3", "python3"},
        {"c#", "csharp"},  {"cs", "csharp"},   {"js", "javascript"},
        {"node", "javascript"}, {"ts", "typescript"}, {"go", "golang"},
        {"rs", "rust"},    {"rb", "ruby"},     {"kt", "kotlin"},
    };
    std::string key = util::to_lower(util::trim(name));
    auto alias = aliases.find(key);
    if (alias != aliases.end()) key = alias->second;
    return by_key(key);
}

std::map<std::string, int> score(const std::string& text) {
    std::map<std::string, int> raw;
    for (const auto& [key, table] : signals()) {
        int total = 0;
        for (const Signal& signal : table) {
            if (std::regex_search(text, signal.pattern)) total += signal.weight;
        }
        raw[key] = total;
    }
    apply_gates(text, raw);
    return raw;
}

std::vector<std::pair<Language, int>> rank(const std::string& text) {
    auto scores = score(text);
    std::vector<std::pair<Language, int>> ranked;
    ranked.reserve(languages().size());
    for (const Language& lang : languages()) {
        auto hit = scores.find(lang.key);
        ranked.emplace_back(lang, hit == scores.end() ? 0 : hit->second);
    }
    // Declaration order breaks ties because stable_sort keeps input order.
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    return ranked;
}

Detection detect(const std::string& text) {
    auto ranked = rank(text);
    if (util::trim(text).empty()) {
        return Detection{std::nullopt, 0, std::move(ranked)};
    }
    const auto& [best, best_score] = ranked.front();
    if (best_score < kThreshold) {
        return Detection{std::nullopt, best_score, std::move(ranked)};
    }
    Detection result{best, best_score, {}};
    result.ranked = std::move(ranked);
    return result;
}

}  // namespace lcpush::detect
