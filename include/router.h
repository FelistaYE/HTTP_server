#pragma once

#include "http_parser.h"
#include "http_response.h"

#include <functional>
#include <string>
#include <vector>

// Route handler function type
using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class Router {
public:
    // Register routes
    void get(const std::string& path, RouteHandler handler);
    void post(const std::string& path, RouteHandler handler);
    void put(const std::string& path, RouteHandler handler);
    void del(const std::string& path, RouteHandler handler);

    // Match request to a handler
    // Returns 404 if no route matches
    HttpResponse handle(const HttpRequest& req) const;

private:
    struct Route {
        HttpMethod method;
        std::string path;
        RouteHandler handler;
    };

    std::vector<Route> routes_;
};
