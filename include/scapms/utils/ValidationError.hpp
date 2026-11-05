#pragma once

#include <stdexcept>
#include <string>

namespace scapms::utils {

class ValidationError : public std::runtime_error {
public:
    explicit ValidationError(const std::string& message) : std::runtime_error(message) {}
};

} // namespace scapms::utils
