#include "router.h"
#include "logger.h"

void Router::get(const std::string& path, RouteHandler handler) {
    routes_.push_back({HttpMethod::GET, path, std::move(handler)});
    LOG_INFO("Route registered: GET " + path);
}

void Router::post(const std::string& path, RouteHandler handler) {
    routes_.push_back({HttpMethod::POST, path, std::move(handler)});
    LOG_INFO("Route registered: POST " + path);
}

void Router::put(const std::string& path, RouteHandler handler) {
    routes_.push_back({HttpMethod::PUT, path, std::move(handler)});
    LOG_INFO("Route registered: PUT " + path);
}

void Router::del(const std::string& path, RouteHandler handler) {
    routes_.push_back({HttpMethod::DELETE_, path, std::move(handler)});
    LOG_INFO("Route registered: DELETE " + path);
}

HttpResponse Router::handle(const HttpRequest& req) const {
    for (const auto& route : routes_) {
        if (route.method == req.method && route.path == req.path) {
            LOG_DEBUG("Route matched: " + req.method_str() + " " + req.path);
            return route.handler(req);
        }
    }

    // no match
    LOG_DEBUG("No route matched: " + req.method_str() + " " + req.path);
    return HttpResponse::not_found();
}
