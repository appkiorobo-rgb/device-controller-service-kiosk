// src/logging/logger.cpp
#include "logging/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>

namespace logging {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (initialized_) {
        return;
    }
    
    logFile_ = std::make_unique<std::ofstream>(logFilePath, std::ios::app);
    if (!logFile_->is_open()) {
        std::cerr << "Failed to open log file: " << logFilePath << std::endl;
        return;
    }
    
    initialized_ = true;
    
    // mutex가 이미 잠겨있으므로 직접 쓰기 (재귀 호출 방지)
    if (logFile_ && logFile_->is_open()) {
        *logFile_ << "[" << getCurrentTimestamp() << "] "
                  << "[INFO ] Logger initialized: " << logFilePath << std::endl;
        logFile_->flush();
    }
    std::cout << "[INFO ] Logger initialized: " << logFilePath << std::endl;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (logFile_ && logFile_->is_open()) {
        // mutex가 이미 잠겨있으므로 직접 쓰기 (재귀 호출 방지)
        *logFile_ << "[" << getCurrentTimestamp() << "] "
                  << "[INFO ] Logger shutting down" << std::endl;
        logFile_->flush();
        std::cout << "[INFO ] Logger shutting down" << std::endl;
        logFile_->close();
    }
    
    initialized_ = false;
}

void Logger::log(LogLevel level, const std::string& message) {
    writeLog(level, message);
}

void Logger::logHex(LogLevel level, const std::string& label, const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        writeLog(level, label + ": (empty)");
        return;
    }
    
    std::ostringstream oss;
    oss << label << " [" << length << " bytes]: ";
    
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(data[i]);
        if (i < length - 1) {
            oss << " ";
        }
    }
    
    writeLog(level, oss.str());
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::WARN, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::debugHex(const std::string& label, const uint8_t* data, size_t length) {
    logHex(LogLevel::DEBUG, label, data, length);
}

void Logger::infoHex(const std::string& label, const uint8_t* data, size_t length) {
    logHex(LogLevel::INFO, label, data, length);
}

Logger::~Logger() {
    // 소멸자에서는 mutex를 잠그지 않고 직접 종료
    // shutdown()을 호출하면 mutex deadlock이 발생할 수 있음
    if (logFile_ && logFile_->is_open()) {
        logFile_->close();
    }
    initialized_ = false;
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

void Logger::writeLog(LogLevel level, const std::string& message) {
    if (!initialized_) {
        // Fallback to console if not initialized
        std::cout << "[" << levelToString(level) << "] " << message << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (logFile_ && logFile_->is_open()) {
        *logFile_ << "[" << getCurrentTimestamp() << "] "
                  << "[" << levelToString(level) << "] "
                  << message << std::endl;
        logFile_->flush();
    }
    
    // Also output to console for debugging
    std::cout << "[" << levelToString(level) << "] " << message << std::endl;
}

} // namespace logging
