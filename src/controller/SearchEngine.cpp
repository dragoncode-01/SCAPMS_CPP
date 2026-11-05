#include "scapms/controller/SearchEngine.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace scapms::controller {

namespace {
bool containsText(const std::string& text, const std::string& query) {
    if (query.empty()) return true;
    if (query.size() > text.size()) return false;
    for (std::size_t start = 0; start + query.size() <= text.size(); ++start) {
        std::size_t offset = 0;
        while (offset < query.size() && text[start + offset] == query[offset]) ++offset;
        if (offset == query.size()) return true;
    }
    return false;
}
}

std::string SearchEngine::lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool SearchEngine::recursiveNameSearch(
    const std::vector<model::PatientRecord*>& records,
    std::size_t index, const std::string& query,
    std::vector<model::PatientRecord*>& results) {
    if (index >= records.size()) return false;
    if (containsText(lower(records[index]->name()), query)) {
        results.push_back(records[index]);
    }
    recursiveNameSearch(records, index + 1, query, results);
    return !results.empty();
}

std::vector<model::PatientRecord*> SearchEngine::search(
    const std::vector<model::PatientRecord*>& records,
    const std::string& field, const std::string& algorithm,
    const std::string& query) {
    std::vector<model::PatientRecord*> results;
    const auto normalizedQuery = lower(query);
    if (normalizedQuery.empty()) throw std::invalid_argument("Enter a search value first.");

    if (algorithm == "binary") {
        if (field != "name") throw std::invalid_argument("Binary search is available for patient name.");
        auto sorted = records;
        for (std::size_t i = 1; i < sorted.size(); ++i) {
            auto* current = sorted[i];
            auto key = lower(current->name());
            std::size_t j = i;
            while (j > 0 && lower(sorted[j - 1]->name()) > key) {
                sorted[j] = sorted[j - 1];
                --j;
            }
            sorted[j] = current;
        }
        std::size_t left = 0;
        std::size_t right = sorted.size();
        while (left < right) {
            const std::size_t middle = left + (right - left) / 2;
            const auto name = lower(sorted[middle]->name());
            if (name < normalizedQuery) left = middle + 1;
            else right = middle;
        }
        for (std::size_t i = left; i < sorted.size(); ++i) {
            const auto name = lower(sorted[i]->name());
            if (name != normalizedQuery) break;
            results.push_back(sorted[i]);
        }
        return results;
    }

    if (algorithm == "recursive") {
        if (field != "name") throw std::invalid_argument("Recursive search is available for patient name.");
        recursiveNameSearch(records, 0, normalizedQuery, results);
        return results;
    }
    if (algorithm != "linear") throw std::invalid_argument("Choose linear, binary, or recursive search.");

    for (auto* patient : records) {
        bool matches = false;
        if (field == "id") matches = lower(patient->id()) == normalizedQuery;
        else if (field == "category") matches = containsText(lower(patient->type()), normalizedQuery);
        else if (field == "name") matches = containsText(lower(patient->name()), normalizedQuery);
        else throw std::invalid_argument("Choose ID, name, or category as the search field.");
        if (matches) results.push_back(patient);
    }
    return results;
}

} // namespace scapms::controller
