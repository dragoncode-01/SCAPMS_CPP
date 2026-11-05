#include "scapms/model/Appointment.hpp"

#include <algorithm>
#include <cctype>

namespace scapms::model {

std::string toString(AppointmentStatus status) {
    switch (status) {
        case AppointmentStatus::Completed: return "Completed";
        case AppointmentStatus::Cancelled: return "Cancelled";
        default: return "Scheduled";
    }
}

AppointmentStatus appointmentStatusFromString(const std::string& value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (normalized == "completed") return AppointmentStatus::Completed;
    if (normalized == "cancelled" || normalized == "canceled") return AppointmentStatus::Cancelled;
    return AppointmentStatus::Scheduled;
}

} // namespace scapms::model
