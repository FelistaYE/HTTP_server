#include "server.h"
#include "logger.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <vector>

Server::Server(uint16_t port, const std::string& static_dir)
    : port_(port), static_server_(static_dir)
{
    init_socket();
    init_epoll();
}

Server::~Server() {
    connections_.clear(); // Close all connections first
    if (epoll_fd_ >= 0) ::close(epoll_fd_);
    if (listen_fd_ >= 0) ::close(listen_fd_);
}

// ─── Initialization ─────────────────────────────────────────────

void Server::init_socket() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // SO_REUSEADDR: allow quick server restart
    int opt = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // TCP_NODELAY: disable Nagle's algorithm to reduce latency
    ::setsockopt(listen_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    set_nonblocking(listen_fd_);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw std::runtime_error("Failed to bind port " + std::to_string(port_));
    }

    if (::listen(listen_fd_, LISTEN_BACKLOG) < 0) {
        throw std::runtime_error("Failed to listen");
    }

    LOG_INFO("Listening on port " + std::to_string(port_));
}

void Server::init_epoll() {
    epoll_fd_ = ::epoll_create1(0);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("Failed to create epoll");
    }

    // Add the listen socket to epoll
    epoll_add(listen_fd_, EPOLLIN);
}

// ─── Event Loop (Core) ──────────────────────────────────────────

void Server::run() {
    running_ = true;

    // Ignore SIGPIPE (prevents crash when writing to a disconnected client)
    ::signal(SIGPIPE, SIG_IGN);

    LOG_INFO("Server started, entering event loop...");

    struct epoll_event events[MAX_EVENTS];

    auto last_cleanup = std::chrono::steady_clock::now();

    while (running_) {
        int n = ::epoll_wait(epoll_fd_, events, MAX_EVENTS, CLEANUP_INTERVAL_MS);

        if (n < 0) {
            if (errno == EINTR) continue; // Interrupted by signal, retry
            LOG_ERROR("epoll_wait error: " + std::string(strerror(errno)));
            break;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == listen_fd_) {
                // New connection incoming
                accept_connections();
            } else if (ev & (EPOLLERR | EPOLLHUP)) {
                // Error or peer hung up
                close_connection(fd);
            } else {
                if (ev & EPOLLIN) {
                    handle_read(fd);
                }
                if (ev & EPOLLOUT) {
                    handle_write(fd);
                }
            }
        }

        // Periodically clean up idle connections
        auto now = std::chrono::steady_clock::now();
        if (now - last_cleanup >= std::chrono::milliseconds(CLEANUP_INTERVAL_MS)) {
            cleanup_idle_connections();
            last_cleanup = now;
        }
    }

    LOG_INFO("Server stopped.");
}

void Server::stop() {
    running_ = false;
}

// ─── Connection Management ──────────────────────────────────────

void Server::accept_connections() {
    // In edge-triggered mode, there may be multiple pending connections per event
    while (true) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = ::accept(listen_fd_,
                                  reinterpret_cast<sockaddr*>(&client_addr),
                                  &addr_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // All pending connections have been accepted
            }
            LOG_ERROR("Accept error: " + std::string(strerror(errno)));
            break;
        }

        set_nonblocking(client_fd);

        // TCP_NODELAY
        int opt = 1;
        ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        // Edge-triggered (EPOLLET): no need for EPOLLONESHOT in single-threaded model
        epoll_add(client_fd, EPOLLIN | EPOLLET);

        connections_[client_fd] = std::make_unique<Connection>(client_fd);

        char ip[INET_ADDRSTRLEN];
        ::inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        LOG_DEBUG("New connection: fd=" + std::to_string(client_fd)
                  + " from " + ip + ":" + std::to_string(ntohs(client_addr.sin_port)));
    }
}

void Server::handle_read(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = *it->second;

    // Edge-triggered: must read until EAGAIN
    while (true) {
        int result = conn.read_data();
        if (result > 0) {
            // New data available, try to parse
            if (conn.parse() && conn.request_complete()) {
                process_request(fd);
                return;
            }
        } else if (result == 0) {
            // Peer closed connection
            close_connection(fd);
            return;
        } else if (result == -1) {
            // EAGAIN, no more data available
            // If we already have partial data, try to parse
            if (conn.request_complete()) {
                process_request(fd);
            }
            return;
        } else {
            // Error
            close_connection(fd);
            return;
        }
    }
}

void Server::handle_write(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = *it->second;

    if (conn.write_data()) {
        // Write complete
        if (conn.request().keep_alive()) {
            // Keep-alive: reset connection and resume listening for reads
            conn.reset_for_next_request();
            epoll_modify(fd, EPOLLIN | EPOLLET);
        } else {
            close_connection(fd);
        }
    }
    // If write is incomplete, EPOLLOUT will fire again
}

void Server::process_request(int fd) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) return;

    Connection& conn = *it->second;
    const HttpRequest& req = conn.request();

    LOG_INFO(req.method_str() + " " + req.path);

    // 1. Try API routes first
    HttpResponse resp = router_.handle(req);

    // 2. If router returned 404, fall back to static files
    // (Simplified: detect 404 by checking serialized response string)
    if (resp.serialize().find("404 Not Found") != std::string::npos) {
        HttpResponse static_resp = static_server_.serve(req);
        if (static_resp.serialize().find("404 Not Found") == std::string::npos) {
            resp = std::move(static_resp);
        }
    }

    // Set keep-alive
    resp.keep_alive(req.keep_alive());

    conn.set_response(resp.serialize());

    // Try to write immediately
    if (!conn.write_data()) {
        // Write incomplete, register for EPOLLOUT
        epoll_modify(fd, EPOLLOUT | EPOLLET);
    } else {
        // Write complete
        if (req.keep_alive()) {
            conn.reset_for_next_request();
            epoll_modify(fd, EPOLLIN | EPOLLET);
        } else {
            close_connection(fd);
        }
    }
}

void Server::cleanup_idle_connections() {
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(IDLE_TIMEOUT_SEC);

    std::vector<int> to_close;
    for (auto& [fd, conn] : connections_) {
        if (now - conn->last_active() >= timeout) {
            to_close.push_back(fd);
        }
    }

    for (int fd : to_close) {
        LOG_DEBUG("Closing idle connection: fd=" + std::to_string(fd));
        close_connection(fd);
    }

    if (!to_close.empty()) {
        LOG_INFO("Cleaned up " + std::to_string(to_close.size()) + " idle connection(s)");
    }
}

void Server::close_connection(int fd) {
    LOG_DEBUG("Closing connection: fd=" + std::to_string(fd));
    epoll_remove(fd);
    connections_.erase(fd); // Connection destructor will close(fd)
}

// ─── Helper Functions ───────────────────────────────────────────

void Server::set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Server::epoll_add(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
}

void Server::epoll_modify(int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
}

void Server::epoll_remove(int fd) {
    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}
