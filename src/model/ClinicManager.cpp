#include "scapms/model/ClinicManager.hpp"

#include "scapms/controller/AppointmentController.hpp"
#include "scapms/utils/FileHandler.hpp"
#include "scapms/utils/ValidationError.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>

namespace scapms::model {

ClinicManager::ClinicManager(std::string dataPath)
    : dataPath_(std::move(dataPath)),
      appointmentController_(std::make_unique<controller::AppointmentController>(database_, idGenerator_)) {
    if (std::filesystem::exists(dataPath_)) reload();
}

ClinicManager::~ClinicManager() = default;

controller::AppointmentController& ClinicManager::appointmentController() noexcept {
    return *appointmentController_;
}

void ClinicManager::validatePatient(const std::string& type, const std::string& name,
                                    int age, const std::string& contact) const {
    if (type != "General" && type != "Emergency" && type != "Follow-Up")
        throw utils::ValidationError("Choose a valid patient category.");
    if (name.empty()) throw utils::ValidationError("Patient name is required.");
    if (age < 0 || age > 120) throw utils::ValidationError("Patient age must be between 0 and 120.");
    if (contact.empty()) throw utils::ValidationError("Patient contact information is required.");
}

void ClinicManager::validatePersistence(const std::vector<PatientSnapshot>& patients,
                                       const std::vector<std::string>& waitingIds) const {
    for (std::size_t i = 0; i < patients.size(); ++i) {
        const auto& patient = patients[i];
        if (patient.id.empty()) throw utils::ValidationError("Saved patient ID cannot be empty.");
        validatePatient(patient.type, patient.name, patient.age, patient.contact);
        for (std::size_t j = i + 1; j < patients.size(); ++j)
            if (patient.id == patients[j].id) throw utils::ValidationError("Duplicate patient ID: " + patient.id);
        for (std::size_t a = 0; a < patient.appointments.size(); ++a) {
            const auto& appointment = patient.appointments[a];
            if (appointment.id.empty() || appointment.patientId != patient.id)
                throw utils::ValidationError("Saved appointment data is inconsistent.");
            for (std::size_t b = a + 1; b < patient.appointments.size(); ++b)
                if (appointment.id == patient.appointments[b].id)
                    throw utils::ValidationError("Duplicate appointment ID: " + appointment.id);
            for (std::size_t p = i + 1; p < patients.size(); ++p)
                for (const auto& other : patients[p].appointments)
                    if (appointment.id == other.id)
                        throw utils::ValidationError("Duplicate appointment ID: " + appointment.id);
        }
    }
    for (std::size_t i = 0; i < waitingIds.size(); ++i) {
        bool foundPatient = false;
        for (const auto& patient : patients) if (patient.id == waitingIds[i]) foundPatient = true;
        if (!foundPatient) throw utils::ValidationError("Waiting queue refers to an unknown patient: " + waitingIds[i]);
        for (std::size_t j = i + 1; j < waitingIds.size(); ++j)
            if (waitingIds[i] == waitingIds[j]) throw utils::ValidationError("Duplicate patient in waiting queue: " + waitingIds[i]);
    }
}

PatientRecord& ClinicManager::registerPatient(const std::string& type, const std::string& name,
                                             int age, const std::string& contact,
                                             const std::string& notes) {
    validatePatient(type, name, age, contact);
    std::string id;
    do { id = idGenerator_.nextPatientId(); } while (database_.findById(id));
    PatientSnapshot absent{type, id, name, age, contact, notes, {}};
    database_.recordUndo(absent, false, false);
    database_.addPatient(makePatient(absent));
    save();
    return *database_.findById(id);
}

void ClinicManager::editPatient(const std::string& id, const std::string& type,
                                const std::string& name, int age,
                                const std::string& contact, const std::string& notes) {
    auto* current = database_.findById(id);
    if (!current) throw utils::ValidationError("Patient record was not found.");
    validatePatient(type, name, age, contact);
    const auto before = current->snapshot();
    database_.recordUndo(before, true, database_.isQueued(id));
    PatientSnapshot updated{type, id, name, age, contact, notes, before.appointments};
    database_.updatePatient(updated);
    save();
}

void ClinicManager::deletePatient(const std::string& id) {
    auto* current = database_.findById(id);
    if (!current) throw utils::ValidationError("Patient record was not found.");
    const auto before = current->snapshot();
    const bool queued = database_.isQueued(id);
    database_.recordUndo(before, true, queued);
    database_.deletePatient(id);
    save();
}

bool ClinicManager::undo() {
    const bool changed = database_.undoLastOperation();
    if (changed) save();
    return changed;
}

void ClinicManager::save() const {
    utils::FileHandler::save(dataPath_, utils::FileHandler::serialize(
        database_.snapshots(), database_.waitingIds()));
}

void ClinicManager::reload() {
    const auto data = utils::FileHandler::parse(utils::FileHandler::read(dataPath_));
    validatePersistence(data.patients, data.waitingIds);
    database_.load(data.patients, data.waitingIds);
}

std::string ClinicManager::exportData() const {
    return utils::FileHandler::serialize(database_.snapshots(), database_.waitingIds());
}

void ClinicManager::importData(const std::string& contents) {
    const auto parsed = utils::FileHandler::parse(contents);
    validatePersistence(parsed.patients, parsed.waitingIds);
    database_.load(parsed.patients, parsed.waitingIds);
    save();
}

} // namespace scapms::model
