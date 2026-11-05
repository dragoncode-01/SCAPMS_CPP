#pragma once

#include <string>

namespace scapms::model {

enum class AppointmentStatus { Scheduled, Completed, Cancelled };

struct Appointment {
    std::string id;
    std::string patientId;
    std::string dateTime;
    std::string reason;
    AppointmentStatus status{AppointmentStatus::Scheduled};
};

std::string toString(AppointmentStatus status);
AppointmentStatus appointmentStatusFromString(const std::string& value);

} // namespace scapms::model
