#pragma once

#include <string>
#include <unordered_map>

// HTTP request methods
enum class HttpMethod { GET, POST, PUT, DELETE_, HEAD, UNKNOWN };

// Parsed HTTP request
struct HttpRequest {
    HttpMethod method = HttpMethod::UNKNOWN;
    std::string path;                                        // e.g. "/api/users"
    std::string query_string;                                // e.g. "id=1&name=test"
    std::string version;                                     // e.g. "HTTP/1.1"
    std::unordered_map<std::string, std::string> headers;    // Header name -> value
    std::string body;

    std::string method_str() const;
    std::string header(const std::string& key) const;
    bool keep_alive() const;
};

// Incremental HTTP request parser (handles incomplete data)
class HttpParser {
public:
    enum class State { REQUEST_LINE, HEADERS, BODY, COMPLETE, ERROR };

    // Feed new data, returns true if parsing is complete
    bool feed(const char* data, size_t len);

    State state() const { return state_; }
    const HttpRequest& request() const { return request_; }

    void reset();

private:
    bool parse_request_line(const std::string& line);
    bool parse_header_line(const std::string& line);

    State state_ = State::REQUEST_LINE;
    HttpRequest request_;
    std::string buffer_;
    size_t content_length_ = 0;
};

HttpMethod str_to_method(const std::string& s);
