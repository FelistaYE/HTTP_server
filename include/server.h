#pragma once

#include "connection.h"
#include "router.h"
#include "static_server.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

class Server {
public:
    Server(uint16_t port, const std::string& static_dir);
    ~Server();

    // Get router reference for registering routes
    Router& router() { return router_; }

    // Start the server (blocking, enters event loop)
    void run();

    // Graceful shutdown
    void stop();

private:
    void init_socket();
    void init_epoll();

    void accept_connections();
    void handle_read(int fd);
    void handle_write(int fd);
    void process_request(int fd);
    void close_connection(int fd);
    void cleanup_idle_connections();

    void set_nonblocking(int fd);
    void epoll_add(int fd, uint32_t events);
    void epoll_modify(int fd, uint32_t events);
    void epoll_remove(int fd);

    uint16_t port_;
    int listen_fd_ = -1;
    int epoll_fd_ = -1;
    bool running_ = false;

    Router router_;
    StaticServer static_server_;

    std::unordered_map<int, std::unique_ptr<Connection>> connections_;

    static constexpr int MAX_EVENTS = 1024;
    static constexpr int LISTEN_BACKLOG = 512;
    static constexpr int IDLE_TIMEOUT_SEC = 60;       // Idle connection timeout (seconds)
    static constexpr int CLEANUP_INTERVAL_MS = 5000;   // Cleanup check interval (milliseconds)
};
