#include "scapms/server/HttpServer.hpp"

#include "scapms/controller/SearchEngine.hpp"
#include "scapms/controller/AppointmentController.hpp"
#include "scapms/utils/FileHandler.hpp"
#include "scapms/utils/ValidationError.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace scapms::server {

namespace {
struct Request {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct Response {
    int status{200};
    std::string contentType{"application/json; charset=utf-8"};
    std::string body;
    std::string extraHeaders;
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string urlDecode(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '+' ) output.push_back(' ');
        else if (input[i] == '%' && i + 2 < input.size()) {
            const int high = hexValue(input[i + 1]);
            const int low = hexValue(input[i + 2]);
            if (high < 0 || low < 0) throw std::runtime_error("Malformed URL-encoded value.");
            output.push_back(static_cast<char>((high << 4) | low));
            i += 2;
        } else output.push_back(input[i]);
    }
    return output;
}

std::map<std::string, std::string> parseForm(const std::string& input) {
    std::map<std::string, std::string> values;
    std::size_t start = 0;
    while (start <= input.size()) {
        const auto end = input.find('&', start);
        const auto item = input.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto equals = item.find('=');
        const auto key = urlDecode(item.substr(0, equals));
        const auto value = equals == std::string::npos ? std::string{} : urlDecode(item.substr(equals + 1));
        if (!key.empty()) values[key] = value;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return values;
}

std::string jsonString(const std::string& value) {
    std::string output = "\"";
    for (unsigned char c : value) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c < 0x20) {
                    static const char* digits = "0123456789abcdef";
                    output += "\\u00";
                    output.push_back(digits[c >> 4]);
                    output.push_back(digits[c & 0x0f]);
                } else output.push_back(static_cast<char>(c));
        }
    }
    output.push_back('"');
    return output;
}

std::string appointmentJson(const model::Appointment& appointment,
                            const model::ClinicDatabase& database) {
    const auto* patient = database.findById(appointment.patientId);
    std::ostringstream out;
    out << "{\"id\":" << jsonString(appointment.id)
        << ",\"patientId\":" << jsonString(appointment.patientId)
        << ",\"patientName\":" << jsonString(patient ? patient->name() : "Unknown patient")
        << ",\"category\":" << jsonString(patient ? patient->type() : "")
        << ",\"dateTime\":" << jsonString(appointment.dateTime)
        << ",\"reason\":" << jsonString(appointment.reason)
        << ",\"status\":" << jsonString(model::toString(appointment.status)) << '}';
    return out.str();
}

std::string patientJson(const model::PatientRecord& patient) {
    int completed = 0;
    int pending = 0;
    for (const auto& appointment : patient.appointments()) {
        if (appointment.status == model::AppointmentStatus::Completed) ++completed;
        if (appointment.status == model::AppointmentStatus::Scheduled) ++pending;
    }
    std::ostringstream out;
    out << "{\"id\":" << jsonString(patient.id())
        << ",\"name\":" << jsonString(patient.name())
        << ",\"age\":" << patient.age()
        << ",\"contact\":" << jsonString(patient.contact())
        << ",\"notes\":" << jsonString(patient.notes())
        << ",\"category\":" << jsonString(patient.type())
        << ",\"priority\":" << jsonString(patient.priorityLabel())
        << ",\"care\":" << jsonString(patient.careInstructions())
        << ",\"appointments\":" << patient.appointments().size()
        << ",\"completedAppointments\":" << completed
        << ",\"pendingAppointments\":" << pending << '}';
    return out.str();
}

std::string patientsJson(const std::vector<model::PatientRecord*>& patients) {
    std::string out = "[";
    for (std::size_t i = 0; i < patients.size(); ++i) {
        if (i) out += ',';
        out += patientJson(*patients[i]);
    }
    return out + ']';
}

std::string patientsJson(const std::vector<const model::PatientRecord*>& patients) {
    std::string out = "[";
    for (std::size_t i = 0; i < patients.size(); ++i) {
        if (i) out += ',';
        out += patientJson(*patients[i]);
    }
    return out + ']';
}

std::string patientsJson(const model::ClinicDatabase& database) {
    std::string out = "[";
    const auto patients = database.patientPointers();
    for (std::size_t i = 0; i < patients.size(); ++i) {
        if (i) out += ',';
        out += patientJson(*patients[i]);
    }
    return out + ']';
}

std::string appointmentsJson(const std::vector<model::Appointment>& appointments,
                             const model::ClinicDatabase& database) {
    std::string out = "[";
    for (std::size_t i = 0; i < appointments.size(); ++i) {
        if (i) out += ',';
        out += appointmentJson(appointments[i], database);
    }
    return out + ']';
}

std::string nowDate() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

std::string headerValue(const std::map<std::string, std::string>& headers,
                        const std::string& name) {
    const auto it = headers.find(lower(name));
    return it == headers.end() ? std::string{} : it->second;
}

std::string require(const std::map<std::string, std::string>& form, const std::string& key) {
    const auto it = form.find(key);
    return it == form.end() ? std::string{} : trim(it->second);
}

int requireInt(const std::map<std::string, std::string>& form, const std::string& key) {
    const auto value = require(form, key);
    if (value.empty()) throw utils::ValidationError("Enter a valid patient age.");
    std::size_t used = 0;
    int result = 0;
    try { result = std::stoi(value, &used); }
    catch (...) { throw utils::ValidationError("Enter a valid patient age."); }
    if (used != value.size()) throw utils::ValidationError("Enter a valid patient age.");
    return result;
}

std::string dashboardJson(const model::ClinicManager& manager) {
    const auto& db = manager.database();
    const auto patients = db.patientPointers();
    const auto appointments = db.appointments();
    int pending = 0;
    int emergencies = 0;
    std::vector<model::Appointment> upcoming;
    for (auto* patient : patients) if (patient->type() == "Emergency") ++emergencies;
    for (const auto& appointment : appointments) {
        if (appointment.status == model::AppointmentStatus::Scheduled) {
            ++pending;
            upcoming.push_back(appointment);
        }
    }
    for (std::size_t i = 1; i < upcoming.size(); ++i) {
        auto current = upcoming[i];
        std::size_t j = i;
        while (j > 0 && current.dateTime < upcoming[j - 1].dateTime) {
            upcoming[j] = upcoming[j - 1];
            --j;
        }
        upcoming[j] = current;
    }
    if (upcoming.size() > 6) upcoming.resize(6);
    std::ostringstream out;
    out << "{\"patients\":" << patients.size()
        << ",\"appointments\":" << appointments.size()
        << ",\"pending\":" << pending
        << ",\"waiting\":" << db.waitingIds().size()
        << ",\"emergencies\":" << emergencies
        << ",\"upcoming\":" << appointmentsJson(upcoming, db) << '}';
    return out.str();
}

std::string reportsJson(const model::ClinicManager& manager) {
    const auto& db = manager.database();
    auto patients = db.patientPointers();
    const auto allAppointments = db.appointments();
    int general = 0, emergency = 0, followUp = 0;
    int totalToday = 0, completedToday = 0, pendingToday = 0, cancelledToday = 0;
    const auto today = nowDate();
    for (auto* patient : patients) {
        if (patient->type() == "General") ++general;
        else if (patient->type() == "Emergency") ++emergency;
        else if (patient->type() == "Follow-Up") ++followUp;
    }
    std::vector<model::Appointment> pending;
    for (const auto& appointment : allAppointments) {
        if (appointment.status == model::AppointmentStatus::Scheduled) pending.push_back(appointment);
        if (appointment.dateTime.substr(0, 10) == today) {
            ++totalToday;
            if (appointment.status == model::AppointmentStatus::Scheduled) ++pendingToday;
            else if (appointment.status == model::AppointmentStatus::Completed) ++completedToday;
            else ++cancelledToday;
        }
    }
    std::vector<const model::PatientRecord*> frequent;
    for (auto* patient : patients) {
        if (patient->appointments().empty()) continue;
        frequent.push_back(patient);
    }
    const auto completedCount = [](const model::PatientRecord* patient) {
        int count = 0;
        for (const auto& appointment : patient->appointments())
            if (appointment.status == model::AppointmentStatus::Completed) ++count;
        return count;
    };
    for (std::size_t i = 1; i < frequent.size(); ++i) {
        auto* current = frequent[i];
        std::size_t j = i;
        while (j > 0 && completedCount(current) > completedCount(frequent[j - 1])) {
            frequent[j] = frequent[j - 1];
            --j;
        }
        frequent[j] = current;
    }
    if (frequent.size() > 5) frequent.resize(5);
    std::ostringstream out;
    out << "{\"categories\":{\"General\":" << general
        << ",\"Emergency\":" << emergency << ",\"Follow-Up\":" << followUp
        << "},\"frequent\":" << patientsJson(frequent)
        << ",\"pendingAppointments\":" << appointmentsJson(pending, db)
        << ",\"daily\":{\"date\":" << jsonString(today)
        << ",\"total\":" << totalToday << ",\"completed\":" << completedToday
        << ",\"pending\":" << pendingToday << ",\"cancelled\":" << cancelledToday << "}}";
    return out.str();
}

Request readRequest(int client) {
    std::string raw;
    raw.reserve(4096);
    std::array<char, 8192> buffer{};
    std::size_t headerEnd = std::string::npos;
    std::size_t contentLength = 0;
    while (true) {
        const auto count = ::recv(client, buffer.data(), buffer.size(), 0);
        if (count <= 0) throw std::runtime_error("Connection ended before request was complete.");
        raw.append(buffer.data(), static_cast<std::size_t>(count));
        if (raw.size() > 8 * 1024 * 1024) throw std::runtime_error("Request is too large.");
        if (headerEnd == std::string::npos) {
            headerEnd = raw.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                const auto firstEnd = raw.find("\r\n");
                if (firstEnd == std::string::npos) throw std::runtime_error("Malformed HTTP request.");
                std::istringstream first(raw.substr(0, firstEnd));
                Request request;
                first >> request.method >> request.path;
                std::string headerLine;
                std::size_t position = firstEnd + 2;
                while (position < headerEnd) {
                    const auto end = raw.find("\r\n", position);
                    if (end == std::string::npos || end > headerEnd) break;
                    const auto colon = raw.find(':', position);
                    if (colon != std::string::npos && colon < end) {
                        request.headers[lower(raw.substr(position, colon - position))] = trim(raw.substr(colon + 1, end - colon - 1));
                    }
                    position = end + 2;
                }
                const auto lengthHeader = headerValue(request.headers, "content-length");
                if (!lengthHeader.empty()) {
                    try { contentLength = static_cast<std::size_t>(std::stoull(lengthHeader)); }
                    catch (...) { throw std::runtime_error("Invalid content length."); }
                }
                if (contentLength > 6 * 1024 * 1024) throw std::runtime_error("Request body is too large.");
                const auto queryAt = request.path.find('?');
                if (queryAt != std::string::npos) {
                    request.query = request.path.substr(queryAt + 1);
                    request.path.resize(queryAt);
                }
                while (raw.size() < headerEnd + 4 + contentLength) {
                    const auto more = ::recv(client, buffer.data(), buffer.size(), 0);
                    if (more <= 0) throw std::runtime_error("Request body was incomplete.");
                    raw.append(buffer.data(), static_cast<std::size_t>(more));
                }
                request.body = raw.substr(headerEnd + 4, contentLength);
                return request;
            }
        }
    }
}

std::string reasonPhrase(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

void sendResponse(int client, const Response& response) {
    std::ostringstream header;
    header << "HTTP/1.1 " << response.status << ' ' << reasonPhrase(response.status) << "\r\n"
           << "Content-Type: " << response.contentType << "\r\n"
           << "Content-Length: " << response.body.size() << "\r\n"
           << "Connection: close\r\n"
           << "Cache-Control: no-store\r\n"
           << "X-Content-Type-Options: nosniff\r\n"
           << "Content-Security-Policy: default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; img-src 'self' data:\r\n"
           << response.extraHeaders << "\r\n";
    const auto packet = header.str() + "\r\n" + response.body;
    std::size_t sent = 0;
    while (sent < packet.size()) {
        const auto count = ::send(client, packet.data() + sent, packet.size() - sent, MSG_NOSIGNAL);
        if (count <= 0) break;
        sent += static_cast<std::size_t>(count);
    }
}

Response jsonResponse(int status, std::string body) {
    Response response;
    response.status = status;
    response.body = std::move(body);
    return response;
}

Response staticResponse(const std::string& webRoot, const std::string& path) {
    std::string filename;
    std::string type;
    if (path == "/" || path == "/index.html") { filename = "index.html"; type = "text/html; charset=utf-8"; }
    else if (path == "/styles.css") { filename = "styles.css"; type = "text/css; charset=utf-8"; }
    else if (path == "/app.js") { filename = "app.js"; type = "application/javascript; charset=utf-8"; }
    else return jsonResponse(404, "{\"error\":\"Page not found.\"}");
    std::ifstream file(webRoot + "/" + filename, std::ios::binary);
    if (!file) return jsonResponse(404, "{\"error\":\"The dashboard files could not be found. Run the app from its project folder.\"}");
    Response response;
    response.contentType = type;
    response.body.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return response;
}

std::string pathSuffix(const std::string& path, const std::string& prefix) {
    if (path.rfind(prefix, 0) != 0) return {};
    return urlDecode(path.substr(prefix.size()));
}

Response handleApi(model::ClinicManager& manager, const Request& request) {
    const auto& db = manager.database();
    if (request.method == "GET" && request.path == "/api/dashboard")
        return jsonResponse(200, dashboardJson(manager));
    if (request.method == "GET" && request.path == "/api/patients")
        return jsonResponse(200, patientsJson(db));
    if (request.method == "GET" && request.path == "/api/appointments")
        return jsonResponse(200, appointmentsJson(manager.appointmentController().all(), db));
    if (request.method == "GET" && request.path == "/api/queue") {
        const auto ids = db.waitingIds();
        std::string out = "[";
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (i) out += ',';
            const auto* patient = db.findById(ids[i]);
            out += "{\"id\":" + jsonString(ids[i]) + ",\"name\":" +
                   jsonString(patient ? patient->name() : "Unknown patient") +
                   ",\"category\":" + jsonString(patient ? patient->type() : "") + '}';
        }
        return jsonResponse(200, out + ']');
    }
    if (request.method == "GET" && request.path == "/api/reports")
        return jsonResponse(200, reportsJson(manager));
    if (request.method == "GET" && request.path == "/api/data/export") {
        Response response;
        response.contentType = "text/plain; charset=utf-8";
        response.body = manager.exportData();
        response.extraHeaders = "Content-Disposition: attachment; filename=\"scapms-data.tsv\"\r\n";
        return response;
    }
    if (request.method == "POST" && request.path == "/api/patients") {
        const auto form = parseForm(request.body);
        auto& patient = manager.registerPatient(require(form, "type"), require(form, "name"),
                                                requireInt(form, "age"), require(form, "contact"),
                                                require(form, "notes"));
        return jsonResponse(201, patientJson(patient));
    }
    if (request.method == "POST" && request.path == "/api/patients/edit") {
        const auto form = parseForm(request.body);
        manager.editPatient(require(form, "id"), require(form, "type"), require(form, "name"),
                            requireInt(form, "age"), require(form, "contact"), require(form, "notes"));
        const auto* patient = manager.database().findById(require(form, "id"));
        return jsonResponse(200, patientJson(*patient));
    }
    if (request.method == "DELETE" && request.path.rfind("/api/patients/", 0) == 0) {
        manager.deletePatient(pathSuffix(request.path, "/api/patients/"));
        return jsonResponse(200, "{\"ok\":true}");
    }
    if (request.method == "POST" && request.path == "/api/search") {
        const auto form = parseForm(request.body);
        const auto results = controller::SearchEngine::search(
            manager.database().patientPointers(), require(form, "field"),
            require(form, "algorithm"), require(form, "query"));
        return jsonResponse(200, patientsJson(results));
    }
    if (request.method == "POST" && request.path == "/api/sort") {
        const auto form = parseForm(request.body);
        manager.database().sortPatients(require(form, "algorithm"), require(form, "field"));
        manager.save();
        return jsonResponse(200, patientsJson(manager.database()));
    }
    if (request.method == "POST" && request.path == "/api/appointments") {
        const auto form = parseForm(request.body);
        const auto appointment = manager.appointmentController().schedule(
            require(form, "patientId"), require(form, "dateTime"), require(form, "reason"));
        manager.save();
        return jsonResponse(201, appointmentJson(appointment, manager.database()));
    }
    if (request.method == "POST" && request.path.rfind("/api/appointments/", 0) == 0) {
        const auto rest = pathSuffix(request.path, "/api/appointments/");
        const auto slash = rest.find('/');
        if (slash == std::string::npos) return jsonResponse(404, "{\"error\":\"Appointment action was not found.\"}");
        const auto id = rest.substr(0, slash);
        const auto action = rest.substr(slash + 1);
        if (action == "cancel") manager.appointmentController().cancel(id);
        else if (action == "complete") manager.appointmentController().markCompleted(id);
        else if (action == "remove") manager.appointmentController().remove(id);
        else return jsonResponse(404, "{\"error\":\"Appointment action was not found.\"}");
        manager.save();
        return jsonResponse(200, "{\"ok\":true}");
    }
    if (request.method == "POST" && request.path == "/api/queue/add") {
        const auto form = parseForm(request.body);
        manager.database().enqueue(require(form, "patientId"));
        manager.save();
        return jsonResponse(200, "{\"ok\":true}");
    }
    if (request.method == "POST" && request.path == "/api/queue/next") {
        const auto id = manager.database().dequeueNext();
        manager.save();
        const auto* patient = manager.database().findById(id);
        if (!patient) return jsonResponse(200, "{\"id\":\"\",\"name\":\"Queue is empty\"}");
        return jsonResponse(200, "{\"id\":" + jsonString(id) + ",\"name\":" + jsonString(patient->name()) + '}');
    }
    if (request.method == "POST" && request.path == "/api/undo") {
        if (!manager.undo()) return jsonResponse(400, "{\"error\":\"There is no administrative operation to undo.\"}");
        return jsonResponse(200, "{\"ok\":true}");
    }
    if (request.method == "POST" && request.path == "/api/save") {
        manager.save();
        return jsonResponse(200, "{\"ok\":true}");
    }
    if (request.method == "POST" && request.path == "/api/load") {
        manager.reload();
        return jsonResponse(200, "{\"ok\":true}");
    }
    if (request.method == "POST" && request.path == "/api/data/import") {
        manager.importData(request.body);
        return jsonResponse(200, "{\"ok\":true}");
    }
    return jsonResponse(404, "{\"error\":\"API route not found.\"}");
}

Response dispatch(model::ClinicManager& manager, const std::string& webRoot,
                  const Request& request) {
    if (request.path.rfind("/api/", 0) == 0) return handleApi(manager, request);
    if (request.method != "GET") return jsonResponse(405, "{\"error\":\"Use the dashboard controls for this action.\"}");
    return staticResponse(webRoot, request.path);
}

} // namespace

HttpServer::HttpServer(model::ClinicManager& manager, std::string webRoot, int port)
    : manager_(manager), webRoot_(std::move(webRoot)), port_(port) {}

int HttpServer::run() {
    const int server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) throw std::runtime_error("Could not create the local web server socket.");
    int reuse = 1;
    ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port_));
    if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        const std::string message = std::string("Could not bind to port ") + std::to_string(port_) + ": " + std::strerror(errno);
        ::close(server);
        throw std::runtime_error(message);
    }
    if (::listen(server, 16) < 0) {
        ::close(server);
        throw std::runtime_error("Could not start listening for dashboard connections.");
    }
    std::cout << "SCAPMS is running at http://127.0.0.1:" << port_ << "\nPress Ctrl+C to stop.\n" << std::flush;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);
        const int client = ::accept(server, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
        if (client < 0) {
            if (errno == EINTR) continue;
            const std::string message = std::string("Local web server stopped: ") + std::strerror(errno);
            ::close(server);
            throw std::runtime_error(message);
        }
        try {
            const auto request = readRequest(client);
            const auto response = dispatch(manager_, webRoot_, request);
            sendResponse(client, response);
        } catch (const utils::ValidationError& error) {
            sendResponse(client, jsonResponse(400, "{\"error\":" + jsonString(error.what()) + '}'));
        } catch (const std::invalid_argument& error) {
            sendResponse(client, jsonResponse(400, "{\"error\":" + jsonString(error.what()) + '}'));
        } catch (const std::exception& error) {
            std::cerr << "Request error: " << error.what() << '\n';
            sendResponse(client, jsonResponse(500, "{\"error\":" + jsonString(error.what()) + '}'));
        }
        ::close(client);
    }
}

} // namespace scapms::server
