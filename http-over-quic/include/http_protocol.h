// http_protocol.h
// HTTP/1.1 Protocol Support for QUIC Transport

#ifndef HTTP_PROTOCOL_H
#define HTTP_PROTOCOL_H

#include <string>
#include <map>
#include <sstream>
#include <cstring>

namespace http {

// HTTP Methods
enum class Method {
    GET,
    POST,
    HEAD,
    PUT,
    DELETE,
    UNKNOWN
};

// HTTP Status Codes
enum class StatusCode {
    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    NO_CONTENT = 204,
    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    NOT_MODIFIED = 304,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    SERVICE_UNAVAILABLE = 503
};

// HTTP Request
struct Request {
    Method method;
    std::string uri;
    std::string version;  // e.g., "HTTP/1.1"
    std::map<std::string, std::string> headers;
    std::string body;

    Request() : method(Method::GET), version("HTTP/1.1") {}

    // Parse HTTP request from string
    bool parse(const std::string& request_str) {
        std::istringstream stream(request_str);
        std::string line;

        // Parse request line: GET /path HTTP/1.1
        if (!std::getline(stream, line)) {
            return false;
        }

        // Remove \r if present
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }

        std::istringstream request_line(line);
        std::string method_str;
        request_line >> method_str >> uri >> version;

        // Parse method
        if (method_str == "GET") method = Method::GET;
        else if (method_str == "POST") method = Method::POST;
        else if (method_str == "HEAD") method = Method::HEAD;
        else if (method_str == "PUT") method = Method::PUT;
        else if (method_str == "DELETE") method = Method::DELETE;
        else method = Method::UNKNOWN;

        // Parse headers
        while (std::getline(stream, line)) {
            // Remove \r if present
            if (!line.empty() && line[line.length() - 1] == '\r') {
                line.erase(line.length() - 1);
            }

            // Empty line marks end of headers
            if (line.empty()) {
                break;
            }

            // Parse header: Name: Value
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                // Trim leading spaces from value
                size_t start = value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    value = value.substr(start);
                }

                headers[name] = value;
            }
        }

        // Rest is body (if any)
        std::string remaining;
        while (std::getline(stream, line)) {
            body += line + "\n";
        }

        return true;
    }

    // Build HTTP request string
    std::string build() const {
        std::ostringstream oss;

        // Request line
        std::string method_str;
        switch (method) {
            case Method::GET: method_str = "GET"; break;
            case Method::POST: method_str = "POST"; break;
            case Method::HEAD: method_str = "HEAD"; break;
            case Method::PUT: method_str = "PUT"; break;
            case Method::DELETE: method_str = "DELETE"; break;
            default: method_str = "GET"; break;
        }

        oss << method_str << " " << uri << " " << version << "\r\n";

        // Headers
        for (const auto& header : headers) {
            oss << header.first << ": " << header.second << "\r\n";
        }

        // End of headers
        oss << "\r\n";

        // Body (if any)
        if (!body.empty()) {
            oss << body;
        }

        return oss.str();
    }
};

// HTTP Response
struct Response {
    std::string version;  // e.g., "HTTP/1.1"
    StatusCode status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;

    Response() : version("HTTP/1.1"), status_code(StatusCode::OK), status_text("OK") {}

    Response(StatusCode code) : version("HTTP/1.1"), status_code(code) {
        // Set status text based on code
        switch (code) {
            case StatusCode::OK: status_text = "OK"; break;
            case StatusCode::CREATED: status_text = "Created"; break;
            case StatusCode::ACCEPTED: status_text = "Accepted"; break;
            case StatusCode::NO_CONTENT: status_text = "No Content"; break;
            case StatusCode::MOVED_PERMANENTLY: status_text = "Moved Permanently"; break;
            case StatusCode::FOUND: status_text = "Found"; break;
            case StatusCode::NOT_MODIFIED: status_text = "Not Modified"; break;
            case StatusCode::BAD_REQUEST: status_text = "Bad Request"; break;
            case StatusCode::UNAUTHORIZED: status_text = "Unauthorized"; break;
            case StatusCode::FORBIDDEN: status_text = "Forbidden"; break;
            case StatusCode::NOT_FOUND: status_text = "Not Found"; break;
            case StatusCode::METHOD_NOT_ALLOWED: status_text = "Method Not Allowed"; break;
            case StatusCode::INTERNAL_SERVER_ERROR: status_text = "Internal Server Error"; break;
            case StatusCode::NOT_IMPLEMENTED: status_text = "Not Implemented"; break;
            case StatusCode::SERVICE_UNAVAILABLE: status_text = "Service Unavailable"; break;
            default: status_text = "Unknown"; break;
        }
    }

    // Parse HTTP response from string
    bool parse(const std::string& response_str) {
        std::istringstream stream(response_str);
        std::string line;

        // Parse status line: HTTP/1.1 200 OK
        if (!std::getline(stream, line)) {
            return false;
        }

        // Remove \r if present
        if (!line.empty() && line[line.length() - 1] == '\r') {
            line.erase(line.length() - 1);
        }

        std::istringstream status_line(line);
        int code;
        status_line >> version >> code;
        status_code = static_cast<StatusCode>(code);

        // Get rest of line as status text
        std::getline(status_line, status_text);
        // Trim leading space
        if (!status_text.empty() && status_text[0] == ' ') {
            status_text = status_text.substr(1);
        }

        // Parse headers
        while (std::getline(stream, line)) {
            // Remove \r if present
            if (!line.empty() && line[line.length() - 1] == '\r') {
                line.erase(line.length() - 1);
            }

            // Empty line marks end of headers
            if (line.empty()) {
                break;
            }

            // Parse header
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string name = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                // Trim leading spaces from value
                size_t start = value.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    value = value.substr(start);
                }

                headers[name] = value;
            }
        }

        return true;
    }

    // Build HTTP response string (headers only)
    std::string buildHeaders() const {
        std::ostringstream oss;

        // Status line
        oss << version << " " << static_cast<int>(status_code) << " " << status_text << "\r\n";

        // Headers
        for (const auto& header : headers) {
            oss << header.first << ": " << header.second << "\r\n";
        }

        // End of headers
        oss << "\r\n";

        return oss.str();
    }
};

// Helper function: Get file extension
inline std::string getFileExtension(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return path.substr(dot_pos + 1);
    }
    return "";
}

// Helper function: Get MIME type from extension
inline std::string getMimeType(const std::string& extension) {
    static const std::map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"txt", "text/plain"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png", "image/png"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"mp4", "video/mp4"},
        {"webm", "video/webm"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"flv", "video/x-flv"},
        {"bin", "application/octet-stream"}
    };

    auto it = mime_types.find(extension);
    if (it != mime_types.end()) {
        return it->second;
    }
    return "application/octet-stream";  // Default
}

} // namespace http

#endif // HTTP_PROTOCOL_H
