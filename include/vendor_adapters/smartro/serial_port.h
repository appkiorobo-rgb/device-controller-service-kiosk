// include/vendor_adapters/smartro/serial_port.h
#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <memory>

namespace device_controller::vendor::smartro {

// SerialPort - Windows COM port communication wrapper
class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    // Open COM port
    // portName: "COM1", "COM2", etc.
    // baudRate: typically 9600 for SMARTRO
    // Returns true if opened successfully
    bool open(const std::string& portName, DWORD baudRate = 115200);

    // Close COM port
    void close();

    // Check if port is open
    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }

    // Read data with timeout
    // Returns number of bytes read, or -1 on error
    int read(std::vector<uint8_t>& buffer, size_t maxBytes, DWORD timeoutMs = 1000);

    // Write data
    // Returns number of bytes written, or -1 on error
    int write(const std::vector<uint8_t>& data);

    // Get available COM ports
    static std::vector<std::string> enumeratePorts();

    // Try to detect SMARTRO terminal by sending device check
    // Returns port name if found, empty string otherwise
    static std::string detectTerminal();

private:
    HANDLE handle_;
    std::string portName_;
    DWORD baudRate_;

    bool configurePort(DWORD baudRate);
};

} // namespace device_controller::vendor::smartro
