#pragma once

#include "scapms/model/PatientRecord.hpp"

#include <string>
#include <vector>

namespace scapms::utils {

struct PersistentData {
    std::vector<model::PatientSnapshot> patients;
    std::vector<std::string> waitingIds;
};

class FileHandler {
public:
    static std::string serialize(const std::vector<model::PatientSnapshot>& patients,
                                 const std::vector<std::string>& waitingIds);
    static PersistentData parse(const std::string& contents);
    static void save(const std::string& path, const std::string& contents);
    static std::string read(const std::string& path);
};

} // namespace scapms::utils
