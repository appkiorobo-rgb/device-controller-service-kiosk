// src/logging/logger.cpp
#include "logging/logger.h"
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

namespace device_controller::logging {

Logger::Logger(const std::string& logDirectory, const std::string& logFileName)
    : logDirectory_(logDirectory)
    , logFileName_(logFileName)
{
    // Create log directory if it doesn't exist
    try {
        std::filesystem::create_directories(logDirectory_);
    } catch (const std::exception&) {
        // Ignore directory creation errors - will try to open file anyway
    }
    openLogFile();
}

Logger::~Logger() {
    closeLogFile();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!logFile_.is_open()) {
        openLogFile();
    }

    // Check if rotation is needed
    if (currentFileSize_ > MAX_FILE_SIZE) {
        rotate();
    }

    std::string formatted = formatLogMessage(level, message);
    logFile_ << formatted << std::endl;
    logFile_.flush();
    currentFileSize_ += formatted.length() + 1;
}

void Logger::rotate() {
    closeLogFile();

    try {
        // Generate rotated filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        std::string rotatedName = logFileName_ + "." + ss.str();

        std::string currentPath = getLogFilePath();
        std::string rotatedPath = logDirectory_ + "/" + rotatedName;

        if (std::filesystem::exists(currentPath)) {
            std::filesystem::rename(currentPath, rotatedPath);
        }
    } catch (const std::exception&) {
        // Ignore rotation errors - continue with new file
    }

    openLogFile();
}

std::string Logger::getLogFilePath() const {
    return logDirectory_ + "/" + logFileName_;
}

std::string Logger::formatLogMessage(LogLevel level, const std::string& message) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();

    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARN"; break;
        case LogLevel::ERR: levelStr = "ERROR"; break;
    }

    ss << " [" << levelStr << "] " << message;
    return ss.str();
}

void Logger::openLogFile() {
    std::string path = getLogFilePath();
    logFile_.open(path, std::ios::app);
    if (logFile_.is_open()) {
        try {
            currentFileSize_ = std::filesystem::exists(path) ? 
                              std::filesystem::file_size(path) : 0;
        } catch (const std::exception&) {
            currentFileSize_ = 0;
        }
    }
}

void Logger::closeLogFile() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

} // namespace device_controller::logging
