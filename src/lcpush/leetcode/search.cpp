#include "lcpush/leetcode/search.hpp"

#include <rapidfuzz/fuzz.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>

#include "lcpush/util/strings.hpp"

namespace lcpush::search {

namespace {

// Numeric ids sort first, ascending, mirroring the Python sort key.
std::tuple<int, int64_t, std::string> sort_key(const Question& question) {
    if (util::is_all_digits(question.id)) {
        return {0, std::stoll(question.id), question.id};
    }
    return {1, 0, question.id};
}

}  // namespace

ProblemIndex build_index(const std::vector<Question>& questions) {
    ProblemIndex index;
    index.questions = questions;
    index.displays.reserve(questions.size());
    index.slugs.reserve(questions.size());
    for (const Question& question : questions) {
        index.displays.push_back(question.display());
        index.slugs.push_back(question.slug);
    }
    return index;
}

std::vector<Question> search(const ProblemIndex& index, const std::string& query,
                             int limit) {
    std::string text = util::trim(query);
    if (text.empty()) {
        std::vector<Question> sorted_questions = index.questions;
        std::stable_sort(sorted_questions.begin(), sorted_questions.end(),
                         [](const Question& a, const Question& b) {
                             return sort_key(a) < sort_key(b);
                         });
        if (sorted_questions.size() > static_cast<size_t>(limit)) {
            sorted_questions.resize(static_cast<size_t>(limit));
        }
        return sorted_questions;
    }

    std::vector<Question> results;
    std::set<size_t> taken;

    if (util::is_all_digits(text)) {
        std::vector<std::pair<size_t, const Question*>> prefixed;
        for (size_t position = 0; position < index.questions.size(); ++position) {
            if (util::starts_with(index.questions[position].id, text)) {
                prefixed.emplace_back(position, &index.questions[position]);
            }
        }
        std::stable_sort(prefixed.begin(), prefixed.end(),
                         [](const auto& a, const auto& b) {
                             return sort_key(*a.second) < sort_key(*b.second);
                         });
        for (const auto& [position, question] : prefixed) {
            if (results.size() >= static_cast<size_t>(limit)) break;
            results.push_back(*question);
            taken.insert(position);
        }
        if (results.size() >= static_cast<size_t>(limit)) return results;
    }

    // Scores are computed over code points, matching Python rapidfuzz.
    std::u32string query_u32 = util::to_u32(text);
    rapidfuzz::fuzz::CachedWRatio<char32_t> scorer(query_u32);
    std::map<size_t, double> scores;
    for (const auto* choices : {&index.displays, &index.slugs}) {
        for (size_t position = 0; position < choices->size(); ++position) {
            if (taken.count(position)) continue;
            double value = scorer.similarity(util::to_u32((*choices)[position]), kScoreCutoff);
            if (value < kScoreCutoff) continue;
            auto hit = scores.find(position);
            if (hit == scores.end() || value > hit->second) {
                scores[position] = value;
            }
        }
    }

    std::vector<std::pair<size_t, double>> ranked(scores.begin(), scores.end());
    std::stable_sort(ranked.begin(), ranked.end(),
                     [&](const auto& a, const auto& b) {
                         if (a.second != b.second) return a.second > b.second;
                         return sort_key(index.questions[a.first]) <
                                sort_key(index.questions[b.first]);
                     });
    for (const auto& [position, value] : ranked) {
        if (results.size() >= static_cast<size_t>(limit)) break;
        results.push_back(index.questions[position]);
    }
    return results;
}

}  // namespace lcpush::search
