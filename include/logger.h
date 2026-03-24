#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    void set_file(const std::string& path);

    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

private:
    Logger() = default;
    void log(LogLevel level, const std::string& msg);
    std::string level_str(LogLevel level) const;
    std::string timestamp() const;

    LogLevel min_level_ = LogLevel::INFO;
    std::ofstream file_;
    std::mutex mtx_;
};

// Convenience macros for logging
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
