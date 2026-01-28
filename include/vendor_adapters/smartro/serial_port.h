// include/vendor_adapters/smartro/serial_port.h
#pragma once

// Windows macro protection
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef INVALID_HANDLE_VALUE
        #define INVALID_HANDLE_VALUE ((void*)(-1))
    #endif
#endif

#include <string>
#include <vector>
#include <cstdint>

namespace smartro {

class SerialPort {
public:
    SerialPort();
    ~SerialPort();
    
    // Prevent copy and move
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&&) = delete;
    SerialPort& operator=(SerialPort&&) = delete;
    
    // Open/close port
    bool open(const std::string& portName, uint32_t baudRate = 115200);
    void close();
    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }
    
    // Read/write data
    bool write(const uint8_t* data, size_t length);
    bool read(uint8_t* buffer, size_t bufferSize, size_t& bytesRead, uint32_t timeoutMs = 1000);
    
    // Configuration
    bool setBaudRate(uint32_t baudRate);
    bool setDataBits(uint8_t dataBits);
    bool setStopBits(uint8_t stopBits);
    bool setParity(uint8_t parity);
    
    // Get port name
    std::string getPortName() const { return portName_; }
    
    // Get baud rate
    uint32_t getBaudRate() const { return baudRate_; }
    
    // Get list of available COM ports
    static std::vector<std::string> getAvailablePorts();
    
    // Save working COM port
    static bool saveWorkingPort(const std::string& portName);
    
    // Load saved COM port
    static std::string loadWorkingPort();
    
private:
    void* handle_;  // HANDLE (declared as void* to minimize windows.h dependency)
    std::string portName_;
    uint32_t baudRate_;
    
    bool configurePort();
    void logError(const std::string& operation);
};

} // namespace smartro
