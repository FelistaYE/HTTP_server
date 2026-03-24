#include "http_response.h"

#include <sstream>

HttpResponse& HttpResponse::status(int code) {
    status_code_ = code;
    return *this;
}

HttpResponse& HttpResponse::header(const std::string& key, const std::string& value) {
    headers_[key] = value;
    return *this;
}

HttpResponse& HttpResponse::content_type(const std::string& type) {
    headers_["Content-Type"] = type;
    return *this;
}

HttpResponse& HttpResponse::body(const std::string& data) {
    body_str_ = data;
    use_binary_ = false;
    return *this;
}

HttpResponse& HttpResponse::body(std::vector<char>&& data) {
    body_bin_ = std::move(data);
    use_binary_ = true;
    return *this;
}

HttpResponse& HttpResponse::json(const std::string& json_str) {
    headers_["Content-Type"] = "application/json; charset=utf-8";
    body_str_ = json_str;
    use_binary_ = false;
    return *this;
}

HttpResponse& HttpResponse::keep_alive(bool enabled) {
    headers_["Connection"] = enabled ? "keep-alive" : "close";
    return *this;
}

std::string HttpResponse::serialize() const {
    const auto& body_data = use_binary_
        ? std::string(body_bin_.begin(), body_bin_.end())
        : body_str_;

    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code_ << " " << status_text() << "\r\n";

    // Auto-add Content-Length
    bool has_cl = headers_.count("Content-Length") > 0;
    if (!has_cl && !body_data.empty()) {
        oss << "Content-Length: " << body_data.size() << "\r\n";
    }

    oss << "Server: HighPerfServer/1.0\r\n";

    for (auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }

    oss << "\r\n";
    oss << body_data;

    return oss.str();
}

// ─── Convenience Constructors ───────────────────────────────────

HttpResponse HttpResponse::ok() {
    return HttpResponse().status(200);
}

HttpResponse HttpResponse::not_found() {
    return HttpResponse()
        .status(404)
        .content_type("text/plain")
        .body("404 Not Found");
}

HttpResponse HttpResponse::bad_request() {
    return HttpResponse()
        .status(400)
        .content_type("text/plain")
        .body("400 Bad Request");
}

HttpResponse HttpResponse::internal_error() {
    return HttpResponse()
        .status(500)
        .content_type("text/plain")
        .body("500 Internal Server Error");
}

std::string HttpResponse::status_text() const {
    switch (status_code_) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}
