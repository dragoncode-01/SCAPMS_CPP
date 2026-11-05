#pragma once

#include "scapms/model/PatientRecord.hpp"

#include <array>
#include <memory>
#include <queue>
#include <stack>
#include <string>
#include <vector>

namespace scapms::model {

struct UndoAction {
    PatientSnapshot before;
    bool patientExistedBefore{};
    bool wasQueued{};
};

class ClinicDatabase {
public:
    PatientRecord* findById(const std::string& id);
    const PatientRecord* findById(const std::string& id) const;
    std::vector<PatientRecord*> patientPointers();
    std::vector<const PatientRecord*> patientPointers() const;
    std::vector<PatientSnapshot> snapshots() const;
    std::vector<Appointment> appointments() const;
    std::size_t patientCount() const noexcept { return patients_.size(); }

    void addPatient(std::unique_ptr<PatientRecord> patient);
    bool updatePatient(const PatientSnapshot& snapshot);
    bool deletePatient(const std::string& id);
    bool restorePatient(const PatientSnapshot& snapshot);
    bool undoLastOperation();
    bool hasUndo() const noexcept { return !undoStack_.empty(); }
    void recordUndo(const PatientSnapshot& before, bool existedBefore, bool wasQueued);

    void sortPatients(const std::string& algorithm, const std::string& field);
    void enqueue(const std::string& patientId);
    std::string dequeueNext();
    std::vector<std::string> waitingIds() const;
    bool isQueued(const std::string& patientId) const;
    void load(const std::vector<PatientSnapshot>& patients,
              const std::vector<std::string>& waitingIds);

private:
    void remember(PatientRecord* patient);
    bool removeFromQueue(const std::string& patientId);
    void restoreQueueEntry(const std::string& patientId);

    std::vector<std::unique_ptr<PatientRecord>> patients_;
    std::queue<std::string> waitingQueue_;
    std::stack<UndoAction> undoStack_;
    std::array<PatientRecord*, 10> recentPatients_{};
    std::size_t recentCount_{};
};

} // namespace scapms::model
