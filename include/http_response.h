#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class HttpResponse {
public:
    HttpResponse& status(int code);
    HttpResponse& header(const std::string& key, const std::string& value);
    HttpResponse& content_type(const std::string& type);
    HttpResponse& body(const std::string& data);
    HttpResponse& body(std::vector<char>&& data);
    HttpResponse& json(const std::string& json_str);
    HttpResponse& keep_alive(bool enabled);

    // Serialize into a sendable byte stream
    std::string serialize() const;

    // Convenience factory methods
    static HttpResponse ok();
    static HttpResponse not_found();
    static HttpResponse bad_request();
    static HttpResponse internal_error();

private:
    int status_code_ = 200;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_str_;
    std::vector<char> body_bin_;
    bool use_binary_ = false;

    std::string status_text() const;
};
