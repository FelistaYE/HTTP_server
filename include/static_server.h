#pragma once

#include "http_parser.h"
#include "http_response.h"

#include <string>
#include <unordered_map>

class StaticServer {
public:
    explicit StaticServer(const std::string& root_dir);

    // Try to serve a static file. Returns the file response if found, 404 otherwise
    HttpResponse serve(const HttpRequest& req) const;

private:
    std::string root_dir_;

    // MIME type mapping
    static const std::unordered_map<std::string, std::string> mime_types_;

    std::string get_mime_type(const std::string& path) const;
    std::string resolve_path(const std::string& uri) const;
    bool is_safe_path(const std::string& path) const;  // Prevent directory traversal attacks
};
