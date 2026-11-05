#include "scapms/model/PatientRecord.hpp"

#include "scapms/model/EmergencyPatient.hpp"
#include "scapms/model/FollowUpPatient.hpp"
#include "scapms/model/GeneralPatient.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace scapms::model {

PatientRecord::PatientRecord(std::string id, std::string name, int age,
                             std::string contact, std::string notes)
    : id_(std::move(id)), name_(std::move(name)), age_(age),
      contact_(std::move(contact)), notes_(std::move(notes)) {}

void PatientRecord::updateDetails(std::string name, int age,
                                  std::string contact, std::string notes) {
    name_ = std::move(name);
    age_ = age;
    contact_ = std::move(contact);
    notes_ = std::move(notes);
}

void PatientRecord::scheduleAppointment(const Appointment& appointment) {
    appointments_.push_back(appointment);
}

bool PatientRecord::cancelAppointment(const std::string& appointmentId) {
    return setAppointmentStatus(appointmentId, AppointmentStatus::Cancelled);
}

bool PatientRecord::removeAppointment(const std::string& appointmentId) {
    for (auto it = appointments_.begin(); it != appointments_.end(); ++it) {
        if (it->id == appointmentId) {
            appointments_.erase(it);
            return true;
        }
    }
    return false;
}

bool PatientRecord::setAppointmentStatus(const std::string& appointmentId,
                                         AppointmentStatus status) {
    for (auto& appointment : appointments_) {
        if (appointment.id == appointmentId) {
            appointment.status = status;
            return true;
        }
    }
    return false;
}

PatientSnapshot PatientRecord::snapshot() const {
    return PatientSnapshot{type(), id_, name_, age_, contact_, notes_, appointments_};
}

void PatientRecord::restoreAppointments(std::vector<Appointment> appointments) {
    appointments_ = std::move(appointments);
}

std::unique_ptr<PatientRecord> makePatient(const std::string& type,
                                          const std::string& id,
                                          const std::string& name,
                                          int age,
                                          const std::string& contact,
                                          const std::string& notes) {
    if (type == "Emergency") {
        return std::make_unique<EmergencyPatient>(id, name, age, contact, notes);
    }
    if (type == "Follow-Up") {
        return std::make_unique<FollowUpPatient>(id, name, age, contact, notes);
    }
    if (type == "General") {
        return std::make_unique<GeneralPatient>(id, name, age, contact, notes);
    }
    throw std::invalid_argument("Unknown patient category.");
}

std::unique_ptr<PatientRecord> makePatient(const PatientSnapshot& snapshot) {
    auto patient = makePatient(snapshot.type, snapshot.id, snapshot.name,
                               snapshot.age, snapshot.contact, snapshot.notes);
    patient->restoreAppointments(snapshot.appointments);
    return patient;
}

} // namespace scapms::model
