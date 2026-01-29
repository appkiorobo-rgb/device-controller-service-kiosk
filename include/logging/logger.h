// include/logging/logger.h
#pragma once

#ifndef LOGGER_H
#define LOGGER_H

// Force Windows macros to be set first (before other headers)
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    // Protect if Windows.h is already included
    #ifndef _WINDOWS_
        // Use standard headers only if Windows.h is not included yet
    #endif
#endif

// Include standard headers only (to avoid conflicts with Windows SDK)
#include <iostream>
#include <string>
#include <mutex>
#include <cstdint>
#include <cstddef>

namespace logging {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERR   // named ERR to avoid Windows macro ERROR (winerror.h)
};

// Simple Logger class - console output only, no file output
class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    void initialize(const std::string& /*logFilePath*/) {
        // Ignore file initialization - console only
    }
    
    void shutdown() {
        // Do nothing
    }
    
    void log(LogLevel level, const std::string& message) {
        std::lock_guard<std::mutex> lock(logMutex_);
        const char* levelStr = levelToString(level);
        std::cout << "[" << levelStr << "] " << message << std::endl;
    }
    
    void logHex(LogLevel level, const std::string& label, const uint8_t* data, size_t length) {
        std::lock_guard<std::mutex> lock(logMutex_);
        const char* levelStr = levelToString(level);
        std::cout << "[" << levelStr << "] " << label << " [" << length << " bytes]: ";
        for (size_t i = 0; i < length; ++i) {
            std::cout << std::hex << static_cast<int>(data[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warn(const std::string& message) { log(LogLevel::WARN, message); }
    void error(const std::string& message) { log(LogLevel::ERR, message); }
    
    void debugHex(const std::string& label, const uint8_t* data, size_t length) {
        logHex(LogLevel::DEBUG, label, data, length);
    }
    
    void infoHex(const std::string& label, const uint8_t* data, size_t length) {
        logHex(LogLevel::INFO, label, data, length);
    }
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    const char* levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::mutex logMutex_;
};

} // namespace logging

#endif // LOGGER_H
