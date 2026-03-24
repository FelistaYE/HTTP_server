#include "http_parser.h"

#include <algorithm>
#include <sstream>

// ─── HttpRequest ─────────────────────────────────────────────────

std::string HttpRequest::method_str() const {
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::DELETE_: return "DELETE";
        case HttpMethod::HEAD:    return "HEAD";
        default:                  return "UNKNOWN";
    }
}

std::string HttpRequest::header(const std::string& key) const {
    // HTTP headers are case-insensitive; use simple lowercase matching
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

    for (auto& [k, v] : headers) {
        std::string lower_k = k;
        std::transform(lower_k.begin(), lower_k.end(), lower_k.begin(), ::tolower);
        if (lower_k == lower_key) return v;
    }
    return "";
}

bool HttpRequest::keep_alive() const {
    std::string conn = header("Connection");
    std::transform(conn.begin(), conn.end(), conn.begin(), ::tolower);

    if (version == "HTTP/1.1") {
        return conn != "close";  // HTTP/1.1 defaults to keep-alive
    }
    return conn == "keep-alive"; // HTTP/1.0 defaults to close
}

// ─── HttpParser ──────────────────────────────────────────────────

HttpMethod str_to_method(const std::string& s) {
    if (s == "GET")    return HttpMethod::GET;
    if (s == "POST")   return HttpMethod::POST;
    if (s == "PUT")    return HttpMethod::PUT;
    if (s == "DELETE") return HttpMethod::DELETE_;
    if (s == "HEAD")   return HttpMethod::HEAD;
    return HttpMethod::UNKNOWN;
}

bool HttpParser::feed(const char* data, size_t len) {
    buffer_.append(data, len);

    while (true) {
        if (state_ == State::REQUEST_LINE || state_ == State::HEADERS) {
            // Look for \r\n
            size_t pos = buffer_.find("\r\n");
            if (pos == std::string::npos) return false; // Incomplete data, wait for more

            std::string line = buffer_.substr(0, pos);
            buffer_.erase(0, pos + 2);

            if (state_ == State::REQUEST_LINE) {
                if (!parse_request_line(line)) {
                    state_ = State::ERROR;
                    return false;
                }
                state_ = State::HEADERS;
            } else {
                // Empty line = end of headers
                if (line.empty()) {
                    std::string cl = request_.header("Content-Length");
                    content_length_ = cl.empty() ? 0 : std::stoul(cl);

                    if (content_length_ > 0) {
                        state_ = State::BODY;
                    } else {
                        state_ = State::COMPLETE;
                        return true;
                    }
                } else {
                    if (!parse_header_line(line)) {
                        state_ = State::ERROR;
                        return false;
                    }
                }
            }
        } else if (state_ == State::BODY) {
            if (buffer_.size() >= content_length_) {
                request_.body = buffer_.substr(0, content_length_);
                buffer_.erase(0, content_length_);
                state_ = State::COMPLETE;
                return true;
            }
            return false; // Wait for more body data
        } else {
            return state_ == State::COMPLETE;
        }
    }
}

bool HttpParser::parse_request_line(const std::string& line) {
    // Format: "GET /path?query HTTP/1.1"
    std::istringstream iss(line);
    std::string method_str, uri, version;
    if (!(iss >> method_str >> uri >> version)) return false;

    request_.method = str_to_method(method_str);
    request_.version = version;

    // Separate path and query string
    size_t qpos = uri.find('?');
    if (qpos != std::string::npos) {
        request_.path = uri.substr(0, qpos);
        request_.query_string = uri.substr(qpos + 1);
    } else {
        request_.path = uri;
    }

    return true;
}

bool HttpParser::parse_header_line(const std::string& line) {
    // Format: "Key: Value"
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;

    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    // Strip leading whitespace from value
    size_t start = value.find_first_not_of(' ');
    if (start != std::string::npos) {
        value = value.substr(start);
    }

    request_.headers[key] = value;
    return true;
}

void HttpParser::reset() {
    state_ = State::REQUEST_LINE;
    request_ = HttpRequest{};
    buffer_.clear();
    content_length_ = 0;
}
