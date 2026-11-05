#include "scapms/controller/AppointmentController.hpp"

#include "scapms/utils/ValidationError.hpp"

#include <cctype>
#include <array>
#include <iomanip>
#include <sstream>

namespace scapms::controller {

namespace {
bool validDateTime(const std::string& value) {
    if (value.size() != 16 || value[4] != '-' || value[7] != '-' ||
        value[10] != 'T' || value[13] != ':') return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13) continue;
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
    }
    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(5, 2));
    const int day = std::stoi(value.substr(8, 2));
    const int hour = std::stoi(value.substr(11, 2));
    const int minute = std::stoi(value.substr(14, 2));
    if (year < 1 || month < 1 || month > 12 || hour > 23 || minute > 59) return false;
    static constexpr std::array<int, 12> monthDays{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int daysInMonth = monthDays[static_cast<std::size_t>(month - 1)] + ((month == 2 && leapYear) ? 1 : 0);
    if (day < 1 || day > daysInMonth) return false;
    return true;
}

struct LocatedAppointment {
    model::PatientRecord* patient{};
    model::Appointment appointment;
};

LocatedAppointment locate(model::ClinicDatabase& database, const std::string& id) {
    for (auto* patient : database.patientPointers()) {
        for (const auto& appointment : patient->appointments()) {
            if (appointment.id == id) return {patient, appointment};
        }
    }
    return {};
}
}

model::Appointment AppointmentController::schedule(const std::string& patientId,
                                                    const std::string& dateTime,
                                                    const std::string& reason) {
    auto* patient = database_.findById(patientId);
    if (!patient) throw utils::ValidationError("Select a registered patient.");
    if (!validDateTime(dateTime)) throw utils::ValidationError("Enter a valid appointment date and time.");
    if (reason.empty()) throw utils::ValidationError("Enter a reason for the appointment.");
    const auto before = patient->snapshot();
    database_.recordUndo(before, true, database_.isQueued(patientId));
    std::string appointmentId;
    bool duplicate = false;
    do {
        appointmentId = ids_.nextAppointmentId();
        duplicate = false;
        for (const auto& existing : database_.appointments()) {
            if (existing.id == appointmentId) { duplicate = true; break; }
        }
    } while (duplicate);
    model::Appointment appointment{appointmentId, patientId, dateTime,
                                   reason, model::AppointmentStatus::Scheduled};
    patient->scheduleAppointment(appointment);
    return appointment;
}

void AppointmentController::cancel(const std::string& appointmentId) {
    const auto located = locate(database_, appointmentId);
    if (!located.patient) throw utils::ValidationError("Appointment was not found.");
    if (located.appointment.status != model::AppointmentStatus::Scheduled)
        throw utils::ValidationError("Only a scheduled appointment can be cancelled.");
    database_.recordUndo(located.patient->snapshot(), true, database_.isQueued(located.patient->id()));
    located.patient->cancelAppointment(appointmentId);
}

void AppointmentController::markCompleted(const std::string& appointmentId) {
    const auto located = locate(database_, appointmentId);
    if (!located.patient) throw utils::ValidationError("Appointment was not found.");
    if (located.appointment.status != model::AppointmentStatus::Scheduled)
        throw utils::ValidationError("Only a scheduled appointment can be completed.");
    database_.recordUndo(located.patient->snapshot(), true, database_.isQueued(located.patient->id()));
    located.patient->setAppointmentStatus(appointmentId, model::AppointmentStatus::Completed);
}

void AppointmentController::remove(const std::string& appointmentId) {
    const auto located = locate(database_, appointmentId);
    if (!located.patient) throw utils::ValidationError("Appointment was not found.");
    database_.recordUndo(located.patient->snapshot(), true, database_.isQueued(located.patient->id()));
    located.patient->removeAppointment(appointmentId);
}

} // namespace scapms::controller
