#pragma once

#include <string>

namespace scapms::utils {

class IDGenerator {
public:
    std::string nextPatientId();
    std::string nextAppointmentId();

private:
    unsigned patientCounter_{1};
    unsigned appointmentCounter_{1};
};

} // namespace scapms::utils
