#pragma once

#include "http_parser.h"

#include <chrono>
#include <string>

// Per-connection state for each TCP connection
class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    // Non-copyable, movable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    int fd() const { return fd_; }

    // Read data from socket. Returns: >0 bytes read, 0 peer closed, -1 EAGAIN, -2 error
    int read_data();

    // Write data to socket. Returns true if all data has been sent
    bool write_data();

    // Parse buffered data
    bool parse();
    bool request_complete() const;
    const HttpRequest& request() const { return parser_.request(); }

    // Set response data to be sent
    void set_response(const std::string& data);
    bool has_pending_write() const { return write_pos_ < write_buf_.size(); }

    // Reset connection state (for keep-alive)
    void reset_for_next_request();

    // Update last active timestamp (called on read/write)
    void touch();

    // Get last active timestamp
    std::chrono::steady_clock::time_point last_active() const { return last_active_; }

private:
    int fd_;
    HttpParser parser_;
    std::string read_buf_;
    std::string write_buf_;
    size_t write_pos_ = 0;
    std::chrono::steady_clock::time_point last_active_ = std::chrono::steady_clock::now();
};
