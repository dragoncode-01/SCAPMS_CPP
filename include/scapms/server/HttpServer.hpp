#pragma once

#include "scapms/model/ClinicManager.hpp"

#include <string>

namespace scapms::server {

class HttpServer {
public:
    HttpServer(model::ClinicManager& manager, std::string webRoot, int port);
    int run();

private:
    model::ClinicManager& manager_;
    std::string webRoot_;
    int port_;
};

} // namespace scapms::server
