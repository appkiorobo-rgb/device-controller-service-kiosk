// include/logging/logger.h
#pragma once

#include <string>
#include <fstream>
#include <memory>
#include <mutex>

namespace logging {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();
    
    void initialize(const std::string& logFilePath);
    void shutdown();
    
    void log(LogLevel level, const std::string& message);
    void logHex(LogLevel level, const std::string& label, const uint8_t* data, size_t length);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
    
    void debugHex(const std::string& label, const uint8_t* data, size_t length);
    void infoHex(const std::string& label, const uint8_t* data, size_t length);
    
private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string levelToString(LogLevel level);
    std::string getCurrentTimestamp();
    void writeLog(LogLevel level, const std::string& message);
    
    std::unique_ptr<std::ofstream> logFile_;
    std::mutex logMutex_;
    bool initialized_ = false;
};

} // namespace logging
