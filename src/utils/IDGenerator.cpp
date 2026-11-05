#include "scapms/utils/IDGenerator.hpp"

#include <iomanip>
#include <sstream>

namespace scapms::utils {

namespace {
std::string formatId(const char* prefix, unsigned value) {
    std::ostringstream stream;
    stream << prefix << '-' << std::setw(4) << std::setfill('0') << value;
    return stream.str();
}
}

std::string IDGenerator::nextPatientId() { return formatId("PT", patientCounter_++); }
std::string IDGenerator::nextAppointmentId() { return formatId("AP", appointmentCounter_++); }

} // namespace scapms::utils
