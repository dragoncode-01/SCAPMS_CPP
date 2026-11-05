#pragma once

#include "scapms/model/Appointment.hpp"

#include <memory>
#include <string>
#include <vector>

namespace scapms::model {

struct PatientSnapshot {
    std::string type;
    std::string id;
    std::string name;
    int age{};
    std::string contact;
    std::string notes;
    std::vector<Appointment> appointments;
};

class Appointable {
public:
    virtual ~Appointable() = default;
    virtual void scheduleAppointment(const Appointment& appointment) = 0;
    virtual bool cancelAppointment(const std::string& appointmentId) = 0;
    virtual bool removeAppointment(const std::string& appointmentId) = 0;
};

class PatientRecord : public Appointable {
public:
    PatientRecord(std::string id, std::string name, int age,
                  std::string contact, std::string notes);
    virtual ~PatientRecord() = default;

    const std::string& id() const noexcept { return id_; }
    const std::string& name() const noexcept { return name_; }
    int age() const noexcept { return age_; }
    const std::string& contact() const noexcept { return contact_; }
    const std::string& notes() const noexcept { return notes_; }
    const std::vector<Appointment>& appointments() const noexcept { return appointments_; }

    void updateDetails(std::string name, int age, std::string contact, std::string notes);
    virtual std::string type() const = 0;
    virtual std::string careInstructions() const = 0;
    virtual std::string priorityLabel() const = 0;

    void scheduleAppointment(const Appointment& appointment) override;
    bool cancelAppointment(const std::string& appointmentId) override;
    bool removeAppointment(const std::string& appointmentId) override;
    bool setAppointmentStatus(const std::string& appointmentId, AppointmentStatus status);
    PatientSnapshot snapshot() const;
    void restoreAppointments(std::vector<Appointment> appointments);

private:
    std::string id_;
    std::string name_;
    int age_{};
    std::string contact_;
    std::string notes_;
    std::vector<Appointment> appointments_;
};

std::unique_ptr<PatientRecord> makePatient(const PatientSnapshot& snapshot);
std::unique_ptr<PatientRecord> makePatient(const std::string& type,
                                          const std::string& id,
                                          const std::string& name,
                                          int age,
                                          const std::string& contact,
                                          const std::string& notes);

} // namespace scapms::model
