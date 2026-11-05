#include "scapms/utils/FileHandler.hpp"

#include "scapms/model/Appointment.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace scapms::utils {

namespace {
std::string encode(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char c : value) {
        if (c == '\\') result += "\\\\";
        else if (c == '\t') result += "\\t";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result.push_back(c);
    }
    return result;
}

std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> fields(1);
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (c == '\t') {
            fields.emplace_back();
        } else if (c == '\\' && i + 1 < line.size()) {
            const char next = line[++i];
            if (next == 't') fields.back().push_back('\t');
            else if (next == 'n') fields.back().push_back('\n');
            else if (next == 'r') fields.back().push_back('\r');
            else fields.back().push_back(next);
        } else {
            fields.back().push_back(c);
        }
    }
    return fields;
}

std::size_t findPatient(const std::vector<model::PatientSnapshot>& patients,
                        const std::string& id) {
    for (std::size_t i = 0; i < patients.size(); ++i) {
        if (patients[i].id == id) return i;
    }
    return patients.size();
}
}

std::string FileHandler::serialize(const std::vector<model::PatientSnapshot>& patients,
                                   const std::vector<std::string>& waitingIds) {
    std::string output = "# SCAPMS tab-separated data v1\n";
    for (const auto& patient : patients) {
        output += "P\t" + encode(patient.type) + "\t" + encode(patient.id) + "\t" +
                  encode(patient.name) + "\t" + std::to_string(patient.age) + "\t" +
                  encode(patient.contact) + "\t" + encode(patient.notes) + "\n";
        for (const auto& appointment : patient.appointments) {
            output += "A\t" + encode(appointment.id) + "\t" + encode(appointment.patientId) +
                      "\t" + encode(appointment.dateTime) + "\t" + encode(appointment.reason) +
                      "\t" + encode(model::toString(appointment.status)) + "\n";
        }
    }
    for (const auto& id : waitingIds) output += "Q\t" + encode(id) + "\n";
    return output;
}

PersistentData FileHandler::parse(const std::string& contents) {
    PersistentData data;
    std::size_t lineStart = 0;
    while (lineStart < contents.size()) {
        const auto lineEnd = contents.find('\n', lineStart);
        const auto length = (lineEnd == std::string::npos ? contents.size() : lineEnd) - lineStart;
        auto line = contents.substr(lineStart, length);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lineStart = lineEnd == std::string::npos ? contents.size() : lineEnd + 1;
        if (line.empty() || line[0] == '#') continue;
        const auto fields = splitFields(line);
        if (fields[0] == "P") {
            if (fields.size() != 7) throw std::runtime_error("Invalid patient row in data file.");
            std::size_t consumed = 0;
            int age = 0;
            try { age = std::stoi(fields[4], &consumed); }
            catch (...) { throw std::runtime_error("Invalid age in data file."); }
            if (consumed != fields[4].size()) throw std::runtime_error("Invalid age in data file.");
            const auto duplicate = findPatient(data.patients, fields[2]);
            if (duplicate != data.patients.size()) throw std::runtime_error("Duplicate patient ID in data file: " + fields[2]);
            data.patients.push_back(model::PatientSnapshot{fields[1], fields[2], fields[3], age,
                                                           fields[5], fields[6], {}});
        } else if (fields[0] == "A") {
            if (fields.size() != 6) throw std::runtime_error("Invalid appointment row in data file.");
            if (fields[5] != "Scheduled" && fields[5] != "Completed" && fields[5] != "Cancelled")
                throw std::runtime_error("Invalid appointment status in data file.");
            const auto patientIndex = findPatient(data.patients, fields[2]);
            if (patientIndex == data.patients.size()) throw std::runtime_error("Appointment references a missing patient: " + fields[2]);
            for (const auto& patient : data.patients)
                for (const auto& existing : patient.appointments)
                    if (existing.id == fields[1]) throw std::runtime_error("Duplicate appointment ID in data file: " + fields[1]);
            data.patients[patientIndex].appointments.push_back(model::Appointment{
                fields[1], fields[2], fields[3], fields[4], model::appointmentStatusFromString(fields[5])});
        } else if (fields[0] == "Q") {
            if (fields.size() != 2) throw std::runtime_error("Invalid waiting queue row in data file.");
            data.waitingIds.push_back(fields[1]);
        } else {
            throw std::runtime_error("Unknown row type in data file.");
        }
    }
    return data;
}

void FileHandler::save(const std::string& path, const std::string& contents) {
    const std::filesystem::path target(path);
    if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path());
    const auto temporary = target.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not open data file for saving: " + path);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) throw std::runtime_error("Could not finish writing data file: " + path);
    }
    std::error_code error;
    std::filesystem::rename(temporary, target, error);
    if (error) {
        std::filesystem::remove(target, error);
        error.clear();
        std::filesystem::rename(temporary, target, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Could not replace data file: " + path);
        }
    }
}

std::string FileHandler::read(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not open data file: " + path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace scapms::utils
