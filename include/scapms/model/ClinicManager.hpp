#pragma once

#include "scapms/model/ClinicDatabase.hpp"
#include "scapms/model/Appointment.hpp"
#include "scapms/utils/IDGenerator.hpp"

#include <string>
#include <memory>
#include <vector>

namespace scapms::controller { class AppointmentController; }

namespace scapms::model {

class ClinicManager {
public:
    explicit ClinicManager(std::string dataPath);
    ~ClinicManager();

    ClinicDatabase& database() noexcept { return database_; }
    const ClinicDatabase& database() const noexcept { return database_; }
    controller::AppointmentController& appointmentController() noexcept;

    PatientRecord& registerPatient(const std::string& type, const std::string& name,
                                   int age, const std::string& contact,
                                   const std::string& notes);
    void editPatient(const std::string& id, const std::string& type,
                     const std::string& name, int age,
                     const std::string& contact, const std::string& notes);
    void deletePatient(const std::string& id);
    bool undo();
    void save() const;
    void reload();
    std::string exportData() const;
    void importData(const std::string& contents);
    std::string dataPath() const { return dataPath_; }

private:
    void validatePatient(const std::string& type, const std::string& name,
                         int age, const std::string& contact) const;
    void validatePersistence(const std::vector<PatientSnapshot>& patients,
                             const std::vector<std::string>& waitingIds) const;
    std::string dataPath_;
    ClinicDatabase database_;
    utils::IDGenerator idGenerator_;
    std::unique_ptr<controller::AppointmentController> appointmentController_;
};

} // namespace scapms::model
