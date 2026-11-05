#pragma once

#include "scapms/model/PatientRecord.hpp"

namespace scapms::model {

class EmergencyPatient final : public PatientRecord {
public:
    using PatientRecord::PatientRecord;
    std::string type() const override { return "Emergency"; }
    std::string careInstructions() const override { return "Immediate triage"; }
    std::string priorityLabel() const override { return "High priority"; }
};

} // namespace scapms::model
