#pragma once

#include "scapms/model/PatientRecord.hpp"

namespace scapms::model {

class GeneralPatient final : public PatientRecord {
public:
    using PatientRecord::PatientRecord;
    std::string type() const override { return "General"; }
    std::string careInstructions() const override { return "Standard consultation"; }
    std::string priorityLabel() const override { return "Normal"; }
};

} // namespace scapms::model
