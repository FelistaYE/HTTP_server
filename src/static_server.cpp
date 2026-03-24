#include "static_server.h"
#include "logger.h"

#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

const std::unordered_map<std::string, std::string> StaticServer::mime_types_ = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css"},
    {".js",   "application/javascript"},
    {".json", "application/json"},
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".svg",  "image/svg+xml"},
    {".ico",  "image/x-icon"},
    {".txt",  "text/plain; charset=utf-8"},
    {".pdf",  "application/pdf"},
    {".woff", "font/woff"},
    {".woff2","font/woff2"},
    {".ttf",  "font/ttf"},
};

StaticServer::StaticServer(const std::string& root_dir)
    : root_dir_(fs::weakly_canonical(fs::absolute(root_dir)).string())
{
    if (!fs::exists(root_dir_)) {
        LOG_WARN("Static directory does not exist: " + root_dir_);
        fs::create_directories(root_dir_);
        LOG_INFO("Created static directory: " + root_dir_);
    }
}

HttpResponse StaticServer::serve(const HttpRequest& req) const {
    if (req.method != HttpMethod::GET && req.method != HttpMethod::HEAD) {
        return HttpResponse::not_found();
    }

    std::string file_path = resolve_path(req.path);

    if (!is_safe_path(file_path)) {
        LOG_WARN("Path traversal attempt blocked: " + req.path);
        return HttpResponse().status(403).body("403 Forbidden");
    }

    if (!fs::exists(file_path)) {
        return HttpResponse::not_found();
    }

    // If it's a directory, try to serve index.html
    if (fs::is_directory(file_path)) {
        file_path += "/index.html";
        if (!fs::exists(file_path)) {
            return HttpResponse::not_found();
        }
    }

    // Read file
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file: " + file_path);
        return HttpResponse::internal_error();
    }

    auto file_size = fs::file_size(file_path);
    std::vector<char> content(file_size);
    file.read(content.data(), static_cast<std::streamsize>(file_size));

    LOG_DEBUG("Serving static file: " + file_path + " (" + std::to_string(file_size) + " bytes)");

    HttpResponse resp;
    resp.status(200)
        .content_type(get_mime_type(file_path))
        .body(std::move(content));

    return resp;
}

std::string StaticServer::get_mime_type(const std::string& path) const {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto it = mime_types_.find(ext);
    if (it != mime_types_.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string StaticServer::resolve_path(const std::string& uri) const {
    // Strip query string
    std::string path = uri;
    size_t qpos = path.find('?');
    if (qpos != std::string::npos) {
        path = path.substr(0, qpos);
    }

    return root_dir_ + path;
}

bool StaticServer::is_safe_path(const std::string& path) const {
    // Ensure the resolved absolute path stays within root_dir_
    try {
        std::string canonical = fs::weakly_canonical(path).string();
        return canonical.find(root_dir_) == 0;
    } catch (...) {
        return false;
    }
}
