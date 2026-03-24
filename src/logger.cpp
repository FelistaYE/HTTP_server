#include "logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    min_level_ = level;
}

void Logger::set_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (file_.is_open()) file_.close();
    file_.open(path, std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "[Logger] Failed to open log file: " << path << "\n";
    }
}

void Logger::debug(const std::string& msg) { log(LogLevel::DEBUG, msg); }
void Logger::info(const std::string& msg)  { log(LogLevel::INFO, msg); }
void Logger::warn(const std::string& msg)  { log(LogLevel::WARN, msg); }
void Logger::error(const std::string& msg) { log(LogLevel::ERROR, msg); }

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < min_level_) return;

    std::string line = timestamp() + " [" + level_str(level) + "] " + msg + "\n";

    std::lock_guard<std::mutex> lock(mtx_);
    // Output to both terminal and log file
    std::ostream& out = (level >= LogLevel::WARN) ? std::cerr : std::cout;
    out << line;

    if (file_.is_open()) {
        file_ << line;
        file_.flush();
    }
}

std::string Logger::level_str(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
    }
    return "?????";
}

std::string Logger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm{};
    localtime_r(&time, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}
