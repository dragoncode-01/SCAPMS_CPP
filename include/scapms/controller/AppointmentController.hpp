#pragma once

#include "scapms/model/ClinicDatabase.hpp"
#include "scapms/utils/IDGenerator.hpp"

#include <string>
#include <vector>

namespace scapms::controller {

class AppointmentController {
public:
    AppointmentController(model::ClinicDatabase& database, utils::IDGenerator& ids)
        : database_(database), ids_(ids) {}

    model::Appointment schedule(const std::string& patientId,
                                const std::string& dateTime,
                                const std::string& reason);
    void cancel(const std::string& appointmentId);
    void markCompleted(const std::string& appointmentId);
    void remove(const std::string& appointmentId);
    std::vector<model::Appointment> all() const { return database_.appointments(); }

private:
    model::ClinicDatabase& database_;
    utils::IDGenerator& ids_;
};

} // namespace scapms::controller
