#include "scapms/model/ClinicDatabase.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace scapms::model {

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string firstAppointmentDate(const PatientRecord& patient) {
    std::string first;
    for (const auto& appointment : patient.appointments()) {
        if (appointment.status == AppointmentStatus::Cancelled) continue;
        if (first.empty() || appointment.dateTime < first) first = appointment.dateTime;
    }
    return first.empty() ? "9999-12-31T23:59" : first;
}
}

void ClinicDatabase::remember(PatientRecord* patient) {
    if (!patient) return;
    std::size_t existing = recentCount_;
    for (std::size_t i = 0; i < recentCount_; ++i) {
        if (recentPatients_[i] == patient) { existing = i; break; }
    }
    if (existing < recentCount_) {
        for (std::size_t i = existing; i > 0; --i) recentPatients_[i] = recentPatients_[i - 1];
    } else {
        const std::size_t last = std::min(recentCount_, recentPatients_.size() - 1);
        for (std::size_t i = last; i > 0; --i) recentPatients_[i] = recentPatients_[i - 1];
        if (recentCount_ < recentPatients_.size()) ++recentCount_;
    }
    recentPatients_[0] = patient;
}

PatientRecord* ClinicDatabase::findById(const std::string& id) {
    for (std::size_t i = 0; i < recentCount_; ++i) {
        if (recentPatients_[i] && recentPatients_[i]->id() == id) {
            auto* cached = recentPatients_[i];
            remember(cached);
            return cached;
        }
    }
    for (auto& patient : patients_) {
        if (patient->id() == id) {
            remember(patient.get());
            return patient.get();
        }
    }
    return nullptr;
}

const PatientRecord* ClinicDatabase::findById(const std::string& id) const {
    for (const auto& patient : patients_) if (patient->id() == id) return patient.get();
    return nullptr;
}

std::vector<PatientRecord*> ClinicDatabase::patientPointers() {
    std::vector<PatientRecord*> result;
    result.reserve(patients_.size());
    for (auto& patient : patients_) result.push_back(patient.get());
    return result;
}

std::vector<const PatientRecord*> ClinicDatabase::patientPointers() const {
    std::vector<const PatientRecord*> result;
    result.reserve(patients_.size());
    for (const auto& patient : patients_) result.push_back(patient.get());
    return result;
}

std::vector<PatientSnapshot> ClinicDatabase::snapshots() const {
    std::vector<PatientSnapshot> result;
    result.reserve(patients_.size());
    for (const auto& patient : patients_) result.push_back(patient->snapshot());
    return result;
}

std::vector<Appointment> ClinicDatabase::appointments() const {
    std::vector<Appointment> result;
    for (const auto& patient : patients_) {
        for (const auto& appointment : patient->appointments()) result.push_back(appointment);
    }
    return result;
}

void ClinicDatabase::addPatient(std::unique_ptr<PatientRecord> patient) {
    if (!patient) throw std::invalid_argument("Patient record cannot be empty.");
    patients_.push_back(std::move(patient));
}

bool ClinicDatabase::updatePatient(const PatientSnapshot& snapshot) {
    for (auto& patient : patients_) {
        if (patient->id() == snapshot.id) {
            auto* previous = patient.get();
            auto replacement = makePatient(snapshot);
            auto* replacementPointer = replacement.get();
            for (std::size_t i = 0; i < recentCount_; ++i) {
                if (recentPatients_[i] == previous) recentPatients_[i] = replacementPointer;
            }
            patient = std::move(replacement);
            return true;
        }
    }
    return false;
}

bool ClinicDatabase::removeFromQueue(const std::string& patientId) {
    bool removed = false;
    std::queue<std::string> retained;
    while (!waitingQueue_.empty()) {
        auto id = waitingQueue_.front();
        waitingQueue_.pop();
        if (id == patientId) removed = true;
        else retained.push(std::move(id));
    }
    waitingQueue_ = std::move(retained);
    return removed;
}

bool ClinicDatabase::deletePatient(const std::string& id) {
    for (auto it = patients_.begin(); it != patients_.end(); ++it) {
        if ((*it)->id() != id) continue;
        const auto* removed = it->get();
        patients_.erase(it);
        for (auto& cached : recentPatients_) if (cached == removed) cached = nullptr;
        while (recentCount_ > 0 && recentPatients_[recentCount_ - 1] == nullptr) --recentCount_;
        removeFromQueue(id);
        return true;
    }
    return false;
}

bool ClinicDatabase::restorePatient(const PatientSnapshot& snapshot) {
    if (findById(snapshot.id)) return updatePatient(snapshot);
    addPatient(makePatient(snapshot));
    return true;
}

void ClinicDatabase::recordUndo(const PatientSnapshot& before, bool existedBefore, bool wasQueued) {
    undoStack_.push(UndoAction{before, existedBefore, wasQueued});
}

bool ClinicDatabase::undoLastOperation() {
    if (undoStack_.empty()) return false;
    const auto action = undoStack_.top();
    undoStack_.pop();
    if (action.patientExistedBefore) {
        restorePatient(action.before);
        if (action.wasQueued) restoreQueueEntry(action.before.id);
    } else {
        deletePatient(action.before.id);
    }
    return true;
}

void ClinicDatabase::sortPatients(const std::string& algorithm, const std::string& field) {
    auto less = [&](const std::unique_ptr<PatientRecord>& a,
                    const std::unique_ptr<PatientRecord>& b) {
        if (field == "name") return lower(a->name()) < lower(b->name());
        if (field == "id") return a->id() < b->id();
        if (field == "age") return a->age() < b->age();
        if (field == "date") return firstAppointmentDate(*a) < firstAppointmentDate(*b);
        throw std::invalid_argument("Choose name, ID, age, or appointment date to sort.");
    };
    if (algorithm == "selection") {
        for (std::size_t i = 0; i < patients_.size(); ++i) {
            std::size_t best = i;
            for (std::size_t j = i + 1; j < patients_.size(); ++j) {
                if (less(patients_[j], patients_[best])) best = j;
            }
            if (best != i) std::swap(patients_[i], patients_[best]);
        }
    } else if (algorithm == "insertion") {
        for (std::size_t i = 1; i < patients_.size(); ++i) {
            auto current = std::move(patients_[i]);
            std::size_t j = i;
            while (j > 0 && less(current, patients_[j - 1])) {
                patients_[j] = std::move(patients_[j - 1]);
                --j;
            }
            patients_[j] = std::move(current);
        }
    } else {
        throw std::invalid_argument("Choose selection sort or insertion sort.");
    }
}

void ClinicDatabase::enqueue(const std::string& patientId) {
    if (!findById(patientId)) throw std::invalid_argument("Patient record was not found.");
    if (isQueued(patientId)) throw std::invalid_argument("This patient is already waiting.");
    waitingQueue_.push(patientId);
}

std::string ClinicDatabase::dequeueNext() {
    if (waitingQueue_.empty()) return {};
    auto id = waitingQueue_.front();
    waitingQueue_.pop();
    return id;
}

std::vector<std::string> ClinicDatabase::waitingIds() const {
    std::vector<std::string> result;
    auto copy = waitingQueue_;
    while (!copy.empty()) {
        result.push_back(copy.front());
        copy.pop();
    }
    return result;
}

bool ClinicDatabase::isQueued(const std::string& patientId) const {
    auto copy = waitingQueue_;
    while (!copy.empty()) {
        if (copy.front() == patientId) return true;
        copy.pop();
    }
    return false;
}

void ClinicDatabase::restoreQueueEntry(const std::string& patientId) {
    if (!isQueued(patientId)) waitingQueue_.push(patientId);
}

void ClinicDatabase::load(const std::vector<PatientSnapshot>& patients,
                          const std::vector<std::string>& waitingIds) {
    patients_.clear();
    waitingQueue_ = {};
    undoStack_ = {};
    recentPatients_.fill(nullptr);
    recentCount_ = 0;
    for (const auto& patient : patients) addPatient(makePatient(patient));
    for (const auto& id : waitingIds) if (findById(id) && !isQueued(id)) waitingQueue_.push(id);
}

} // namespace scapms::model
