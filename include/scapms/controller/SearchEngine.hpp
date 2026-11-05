#pragma once

#include "scapms/model/PatientRecord.hpp"

#include <string>
#include <vector>

namespace scapms::controller {

class SearchEngine {
public:
    static std::vector<model::PatientRecord*> search(
        const std::vector<model::PatientRecord*>& records,
        const std::string& field,
        const std::string& algorithm,
        const std::string& query);

private:
    static std::string lower(std::string value);
    static bool recursiveNameSearch(const std::vector<model::PatientRecord*>& records,
                                    std::size_t index, const std::string& query,
                                    std::vector<model::PatientRecord*>& results);
};

} // namespace scapms::controller
