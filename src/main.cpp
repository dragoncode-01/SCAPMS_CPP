#include "scapms/model/ClinicManager.hpp"
#include "scapms/server/HttpServer.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::filesystem::path projectRoot(const char* executable) {
    std::error_code error;
    auto path = std::filesystem::weakly_canonical(std::filesystem::absolute(executable, error), error);
    if (!error) {
        auto parent = path.parent_path();
        if (parent.filename() == "build") parent = parent.parent_path();
        if (std::filesystem::exists(parent / "web" / "index.html")) return parent;
    }
    const auto current = std::filesystem::current_path();
    if (std::filesystem::exists(current / "web" / "index.html")) return current;
    throw std::runtime_error("Could not locate the SCAPMS project folder. Start the app from its project directory.");
}
}

int main(int argc, char** argv) {
    try {
        int port = 8080;
        if (argc > 1) {
            const std::string arg = argv[1];
            if (arg == "--help" || arg == "-h") {
                std::cout << "SCAPMS clinic dashboard\nUsage: scapms [port]\nDefault port: 8080\n";
                return 0;
            }
            std::size_t used = 0;
            port = std::stoi(arg, &used);
            if (used != arg.size() || port < 1024 || port > 65535)
                throw std::runtime_error("Choose a port between 1024 and 65535.");
        }
        const auto root = projectRoot(argv[0]);
        scapms::model::ClinicManager manager((root / "data" / "scapms.tsv").string());
        scapms::server::HttpServer server(manager, (root / "web").string(), port);
        return server.run();
    } catch (const std::exception& error) {
        std::cerr << "SCAPMS: " << error.what() << '\n';
        return 1;
    }
}
