// src/vendor_adapters/smartro/serial_port.cpp
// logger.h�?가??먼�? include?�여 Windows SDK 충돌 방�?
#include "logging/logger.h"
#include "vendor_adapters/smartro/serial_port.h"
#include <windows.h>
#include <string>
#include <algorithm>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

namespace smartro {

SerialPort::SerialPort() 
    : handle_(INVALID_HANDLE_VALUE)
    , baudRate_(115200)
    , dataBits_(8)
    , stopBits_(1)
    , parity_(0) {  // NOPARITY
}

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& portName, uint32_t baudRate) {
    if (isOpen()) {
        logging::Logger::getInstance().warn("Serial port already open: " + portName_);
        close();
    }
    
    portName_ = portName;
    baudRate_ = baudRate;
    
    // COM ?�트 ?�름 변??(COM3 -> \\.\COM3)
    std::string fullPortName = portName;
    if (portName.find("\\\\.\\") != 0) {
        fullPortName = "\\\\.\\" + portName;
    }
    
    logging::Logger::getInstance().debug("Opening serial port: " + fullPortName + " (Baud: " + std::to_string(baudRate) + ")");
    
    // ?�트 ?�기�?별도 ?�레?�에???�행?�여 ?�?�아???�정
    std::atomic<bool> openSuccess(false);
    std::atomic<bool> openComplete(false);
    HANDLE openedHandle = INVALID_HANDLE_VALUE;
    
    std::thread openThread([&]() {
        openedHandle = CreateFileA(
            fullPortName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        
        if (openedHandle != INVALID_HANDLE_VALUE) {
            openSuccess = true;
        }
        openComplete = true;
    });
    
    // ?�?�아?? 2�?
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!openComplete && std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (!openComplete) {
        // ?�?�아??발생 - ?�레?��? ?�직 ?�행 중이�?종료 ?��?
        logging::Logger::getInstance().warn("Port open timeout for " + portName + ", trying next port...");
        // ?�레?��? 종료???�까지 기다리�? ?�고 계속 진행
        // (?�트가 ?�리�??�중???�을 ???�음)
        openThread.detach();
        return false;
    }
    
    openThread.join();
    
    if (!openSuccess || openedHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED || error == ERROR_FILE_NOT_FOUND) {
            logging::Logger::getInstance().warn("Port " + portName + " is not available (error: " + std::to_string(error) + ")");
        } else {
            logError("Failed to open serial port");
        }
        return false;
    }
    
    handle_ = openedHandle;
    
    if (!configurePort()) {
        logError("Failed to configure serial port");
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }
    
    logging::Logger::getInstance().debug("Serial port opened successfully: " + portName_);
    return true;
}

void SerialPort::close() {
    if (isOpen()) {
        logging::Logger::getInstance().debug("Closing serial port: " + portName_);
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = INVALID_HANDLE_VALUE;
        portName_.clear();
    }
}

bool SerialPort::write(const uint8_t* data, size_t length) {
    if (!isOpen()) {
        logging::Logger::getInstance().error("Cannot write: serial port not open");
        return false;
    }
    
    if (!data || length == 0) {
        logging::Logger::getInstance().warn("Attempted to write empty data");
        return false;
    }
    
    logging::Logger::getInstance().debugHex("Serial TX", data, length);
    
    DWORD bytesWritten = 0;
    BOOL result = WriteFile(
        static_cast<HANDLE>(handle_),
        data,
        static_cast<DWORD>(length),
        &bytesWritten,
        nullptr
    );
    
    if (!result || bytesWritten != length) {
        logError("Failed to write data");
        return false;
    }
    
    logging::Logger::getInstance().debug("Written " + std::to_string(bytesWritten) + " bytes");
    return true;
}

bool SerialPort::read(uint8_t* buffer, size_t bufferSize, size_t& bytesRead, uint32_t timeoutMs) {
    if (!isOpen()) {
        logging::Logger::getInstance().error("Cannot read: serial port not open");
        return false;
    }
    
    if (!buffer || bufferSize == 0) {
        logging::Logger::getInstance().warn("Invalid read buffer");
        return false;
    }
    
    // ?�?�아???�정
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = timeoutMs;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    
    SetCommTimeouts(static_cast<HANDLE>(handle_), &timeouts);
    
    DWORD bytesReadDword = 0;
    BOOL result = ReadFile(
        static_cast<HANDLE>(handle_),
        buffer,
        static_cast<DWORD>(bufferSize),
        &bytesReadDword,
        nullptr
    );
    
    bytesRead = static_cast<size_t>(bytesReadDword);
    
    if (!result) {
        DWORD error = GetLastError();
        if (error == ERROR_OPERATION_ABORTED || error == WAIT_TIMEOUT) {
            logging::Logger::getInstance().debug("Read timeout after " + std::to_string(timeoutMs) + "ms");
            return false;
        }
        // ERROR_ACCESS_DENIED (5): COM ??? ?? ????? ??????,
        // ??? ?????, ?? ??. ?? ?? ??? ?? 5?? ? ?? ??.
        if (error == 5) {
            static std::chrono::steady_clock::time_point lastLogTime;
            auto now = std::chrono::steady_clock::now();
            if (now - lastLogTime >= std::chrono::seconds(5)) {
                lastLogTime = now;
                logging::Logger::getInstance().warn(
                    "Serial read failed: Access denied (error 5). "
                    "Port may be in use by another process, disconnected, or no permission.");
            }
            return false;
        }
        logError("Failed to read data");
        return false;
    }
    
    // 1바이???�기??로그 출력?��? ?�음 (?�무 많�? 로그 방�?)
    // ?�러 바이???�을 ?�만 로그 출력
    if (bytesRead > 0 && bytesRead > 1) {
        logging::Logger::getInstance().debugHex("Serial RX", buffer, bytesRead);
    }
    
    return bytesRead > 0;
}

bool SerialPort::setBaudRate(uint32_t baudRate) {
    baudRate_ = baudRate;
    if (isOpen()) {
        return configurePort();
    }
    return true;
}

bool SerialPort::setDataBits(uint8_t dataBits) {
    // 구현 ?�략 (?�요??추�?)
    dataBits_ = dataBits;
    if (isOpen()) return configurePort();
    return true;
}

bool SerialPort::setStopBits(uint8_t stopBits) {
    // 구현 ?�략 (?�요??추�?)
    stopBits_ = stopBits;
    if (isOpen()) return configurePort();
    return true;
}

bool SerialPort::setParity(uint8_t parity) {
    // 구현 ?�략 (?�요??추�?)
    parity_ = parity;
    if (isOpen()) return configurePort();
    return true;
}

bool SerialPort::configurePort() {
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    if (!GetCommState(static_cast<HANDLE>(handle_), &dcb)) {
        logError("Failed to get comm state");
        return false;
    }
    
    // 기본 ?�정
    dcb.BaudRate = baudRate_;
    dcb.ByteSize = dataBits_;
    dcb.Parity = (parity_ == 1) ? ODDPARITY : (parity_ == 2) ? EVENPARITY : NOPARITY;
    dcb.StopBits = (stopBits_ == 2) ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = (parity_ != 0);
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
    
    if (!SetCommState(static_cast<HANDLE>(handle_), &dcb)) {
        logError("Failed to set comm state");
        return false;
    }
    
    // 버퍼 ?�기 ?�정
    SetupComm(static_cast<HANDLE>(handle_), 4096, 4096);
    
    // ?�?�아???�정
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    
    if (!SetCommTimeouts(static_cast<HANDLE>(handle_), &timeouts)) {
        logError("Failed to set comm timeouts");
        return false;
    }
    
    logging::Logger::getInstance().debug("Serial port configured: BaudRate=" + std::to_string(baudRate_));
    return true;
}

void SerialPort::logError(const std::string& operation) {
    DWORD error = GetLastError();
    std::string errorMsg = operation + " failed. Error code: " + std::to_string(error);
    logging::Logger::getInstance().error(errorMsg);
}

std::vector<std::string> SerialPort::getAvailablePorts(bool registryOnly) {
    std::vector<std::string> ports;
    
    // ?��??�트리에??COM ?�트 목록 ?�기
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
                      "HARDWARE\\DEVICEMAP\\SERIALCOMM", 
                      0, 
                      KEY_READ, 
                      &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        char valueName[256];
        char data[256];
        DWORD valueNameSize, dataSize, type;
        
        while (true) {
            valueNameSize = sizeof(valueName);
            dataSize = sizeof(data);
            
            if (RegEnumValueA(hKey, 
                             index, 
                             valueName, 
                             &valueNameSize, 
                             nullptr, 
                             &type, 
                             (LPBYTE)data, 
                             &dataSize) != ERROR_SUCCESS) {
                break;
            }
            
            if (type == REG_SZ && dataSize > 0) {
                std::string portName(data);
                // COM ?�트 ?�름�?추출 (?? "COM3")
                if (portName.find("COM") == 0) {
                    ports.push_back(portName);
                }
            }
            
            index++;
        }
        
        RegCloseKey(hKey);
    }
    
    // ?��??�트리에??찾�? 못한 경우, COM1~COM20까�? ?�도
    if (!registryOnly && ports.empty()) {
        for (int i = 1; i <= 20; ++i) {
            std::string portName = "COM" + std::to_string(i);
            std::string fullPortName = "\\\\.\\" + portName;
            
            HANDLE hPort = CreateFileA(
                fullPortName.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );
            
            if (hPort != INVALID_HANDLE_VALUE) {
                CloseHandle(hPort);
                ports.push_back(portName);
            }
        }
    }
    
    return ports;
}

bool SerialPort::saveWorkingPort(const std::string& portName) {
    // Windows API�??�용?�여 ?�일 ?�기 (fstream ?�??
    HANDLE hFile = CreateFileA(
        "smartro_port.cfg",
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        logging::Logger::getInstance().warn("Failed to save working port to file");
        return false;
    }
    
    DWORD bytesWritten = 0;
    WriteFile(hFile, portName.c_str(), static_cast<DWORD>(portName.length()), &bytesWritten, nullptr);
    CloseHandle(hFile);
    
    logging::Logger::getInstance().info("Saved working port: " + portName);
    return true;
}

std::string SerialPort::loadWorkingPort() {
    // Windows API�??�용?�여 ?�일 ?�기 (fstream ?�??
    HANDLE hFile = CreateFileA(
        "smartro_port.cfg",
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }
    
    char buffer[256] = {0};
    DWORD bytesRead = 0;
    ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
    CloseHandle(hFile);
    
    std::string portName(buffer, bytesRead);
    
    // 공백 ?�거
    portName.erase(portName.find_last_not_of(" \t\n\r\f\v") + 1);
    
    if (!portName.empty()) {
        logging::Logger::getInstance().info("Loaded saved port: " + portName);
    }
    
    return portName;
}

} // namespace smartro
