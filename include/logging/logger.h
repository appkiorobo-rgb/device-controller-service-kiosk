// include/logging/logger.h
#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <chrono>

namespace device_controller::logging {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERR  // ERROR conflicts with Windows.h macro
};

// Logger - file-based logging with rotation
// Logs are for operators and developers, not for clients
class Logger {
public:
    Logger(const std::string& logDirectory, const std::string& logFileName);
    ~Logger();

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warning(const std::string& message) { log(LogLevel::WARNING, message); }
    void error(const std::string& message) { log(LogLevel::ERR, message); }

    // Rotate log file (called by LogRotator)
    void rotate();

private:
    std::string logDirectory_;
    std::string logFileName_;
    std::ofstream logFile_;
    std::mutex mutex_;
    size_t currentFileSize_{0};
    static constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB

    std::string getLogFilePath() const;
    std::string formatLogMessage(LogLevel level, const std::string& message) const;
    void openLogFile();
    void closeLogFile();
};

} // namespace device_controller::logging
