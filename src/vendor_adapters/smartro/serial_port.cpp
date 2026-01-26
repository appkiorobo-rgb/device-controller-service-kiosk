// src/vendor_adapters/smartro/serial_port.cpp
#include "vendor_adapters/smartro/serial_port.h"
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")

namespace device_controller::vendor::smartro {

SerialPort::SerialPort() : handle_(INVALID_HANDLE_VALUE), baudRate_(9600) {
}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& portName, DWORD baudRate) {
    if (isOpen()) {
        close();
    }

    portName_ = portName;
    baudRate_ = baudRate;

    std::string fullPortName = "\\\\.\\" + portName;
    handle_ = CreateFile(
        fullPortName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    return configurePort(baudRate);
}

void SerialPort::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    portName_.clear();
}

bool SerialPort::configurePort(DWORD baudRate) {
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);

    if (!GetCommState(handle_, &dcb)) {
        return false;
    }

    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fAbortOnError = FALSE;

    if (!SetCommState(handle_, &dcb)) {
        return false;
    }

    // Set timeouts
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(handle_, &timeouts)) {
        return false;
    }

    return true;
}

int SerialPort::read(std::vector<uint8_t>& buffer, size_t maxBytes, DWORD timeoutMs) {
    if (!isOpen()) {
        return -1;
    }

    // Update read timeout
    COMMTIMEOUTS timeouts = {0};
    GetCommTimeouts(handle_, &timeouts);
    timeouts.ReadTotalTimeoutConstant = timeoutMs;
    SetCommTimeouts(handle_, &timeouts);

    buffer.resize(maxBytes);
    DWORD bytesRead = 0;

    if (!ReadFile(handle_, buffer.data(), static_cast<DWORD>(maxBytes), &bytesRead, nullptr)) {
        return -1;
    }

    buffer.resize(bytesRead);
    return static_cast<int>(bytesRead);
}

int SerialPort::write(const std::vector<uint8_t>& data) {
    if (!isOpen() || data.empty()) {
        return -1;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr)) {
        return -1;
    }

    return static_cast<int>(bytesWritten);
}

std::vector<std::string> SerialPort::enumeratePorts() {
    std::vector<std::string> ports;

    // Query registry for COM ports
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char valueName[256];
        char data[256];
        DWORD valueNameSize;
        DWORD dataSize;
        DWORD type;

        while (true) {
            valueNameSize = sizeof(valueName);
            dataSize = sizeof(data);
            LONG result = RegEnumValue(hKey, index, valueName, &valueNameSize, nullptr, &type, reinterpret_cast<LPBYTE>(data), &dataSize);
            if (result != ERROR_SUCCESS) {
                break;
            }

            if (type == REG_SZ && dataSize > 0) {
                std::string portName(data, dataSize - 1); // Exclude null terminator
                // Filter out non-COM ports
                if (portName.length() >= 3 && portName.substr(0, 3) == "COM") {
                    ports.push_back(portName);
                }
            }
            index++;
        }

        RegCloseKey(hKey);
    }

    // Sort ports numerically (COM1, COM2, ..., COM10, COM11, ...)
    std::sort(ports.begin(), ports.end(), [](const std::string& a, const std::string& b) {
        try {
            int numA = std::stoi(a.substr(3));
            int numB = std::stoi(b.substr(3));
            return numA < numB;
        } catch (...) {
            return a < b; // Fallback to string comparison
        }
    });

    return ports;
}

std::string SerialPort::detectTerminal() {
    auto ports = enumeratePorts();
    
    for (const auto& portName : ports) {
        SerialPort testPort;
        if (testPort.open(portName, 9600)) {
            // Try to send device check command (Job Code A)
            // This is a simple test - actual detection should be done by protocol layer
            // For now, just check if port opens successfully
            // Real detection will be done by SMARTROProtocol layer
            testPort.close();
            return portName;
        }
    }

    return "";
}

} // namespace device_controller::vendor::smartro
