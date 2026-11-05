#pragma once

#include "scapms/model/PatientRecord.hpp"

namespace scapms::model {

class FollowUpPatient final : public PatientRecord {
public:
    using PatientRecord::PatientRecord;
    std::string type() const override { return "Follow-Up"; }
    std::string careInstructions() const override { return "Review previous care plan"; }
    std::string priorityLabel() const override { return "Follow-up"; }
};

} // namespace scapms::model
