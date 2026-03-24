#include "connection.h"
#include "logger.h"

#include <unistd.h>
#include <sys/socket.h>
#include <cerrno>
#include <utility>

static constexpr size_t READ_BUF_SIZE = 4096;

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_), parser_(std::move(other.parser_)),
      read_buf_(std::move(other.read_buf_)),
      write_buf_(std::move(other.write_buf_)),
      write_pos_(other.write_pos_)
{
    other.fd_ = -1;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = other.fd_;
        parser_ = std::move(other.parser_);
        read_buf_ = std::move(other.read_buf_);
        write_buf_ = std::move(other.write_buf_);
        write_pos_ = other.write_pos_;
        other.fd_ = -1;
    }
    return *this;
}

int Connection::read_data() {
    char buf[READ_BUF_SIZE];
    ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);

    if (n > 0) {
        read_buf_.append(buf, n);
        touch();
        return static_cast<int>(n);
    } else if (n == 0) {
        return 0; // Peer closed connection
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -1; // No more data available for now
        }
        return -2; // Actual error
    }
}

bool Connection::write_data() {
    while (write_pos_ < write_buf_.size()) {
        ssize_t n = ::send(fd_,
                           write_buf_.data() + write_pos_,
                           write_buf_.size() - write_pos_,
                           MSG_NOSIGNAL);

        if (n > 0) {
            write_pos_ += n;
            touch();
        } else if (n == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false; // Will retry later
            }
            LOG_ERROR("Write error on fd " + std::to_string(fd_));
            return false;
        }
    }
    return true; // All data written
}

bool Connection::parse() {
    if (read_buf_.empty()) return false;
    bool complete = parser_.feed(read_buf_.data(), read_buf_.size());
    read_buf_.clear(); // Data has been handed off to parser
    return complete;
}

bool Connection::request_complete() const {
    return parser_.state() == HttpParser::State::COMPLETE;
}

void Connection::set_response(const std::string& data) {
    write_buf_ = data;
    write_pos_ = 0;
}

void Connection::reset_for_next_request() {
    parser_.reset();
    read_buf_.clear();
    write_buf_.clear();
    write_pos_ = 0;
    touch();
}

void Connection::touch() {
    last_active_ = std::chrono::steady_clock::now();
}
