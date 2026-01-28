// src/vendor_adapters/smartro/serial_port.cpp
// logger.hÎ•?Í∞Ä??Î®ºÏ? include?òÏó¨ Windows SDK Ï∂©Îèå Î∞©Ï?
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
    , baudRate_(115200) {
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
    
    // COM ?¨Ìä∏ ?¥Î¶Ñ Î≥Ä??(COM3 -> \\.\COM3)
    std::string fullPortName = portName;
    if (portName.find("\\\\.\\") != 0) {
        fullPortName = "\\\\.\\" + portName;
    }
    
    logging::Logger::getInstance().debug("Opening serial port: " + fullPortName + " (Baud: " + std::to_string(baudRate) + ")");
    
    // ?¨Ìä∏ ?¥Í∏∞Î•?Î≥ÑÎèÑ ?§Î†à?úÏóê???§Ìñâ?òÏó¨ ?Ä?ÑÏïÑ???§Ï†ï
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
    
    // ?Ä?ÑÏïÑ?? 2Ï¥?
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!openComplete && std::chrono::steady_clock::now() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (!openComplete) {
        // ?Ä?ÑÏïÑ??Î∞úÏÉù - ?§Î†à?úÍ? ?ÑÏßÅ ?§Ìñâ Ï§ëÏù¥Î©?Ï¢ÖÎ£å ?ÄÍ∏?
        logging::Logger::getInstance().warn("Port open timeout for " + portName + ", trying next port...");
        // ?§Î†à?úÍ? Ï¢ÖÎ£å???åÍπåÏßÄ Í∏∞Îã§Î¶¨Ï? ?äÍ≥† Í≥ÑÏÜç ÏßÑÌñâ
        // (?¨Ìä∏Í∞Ä ?¥Î¶¨Î©??òÏ§ë???´ÏùÑ ???àÏùå)
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
    
    // ?Ä?ÑÏïÑ???§Ï†ï
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
        logError("Failed to read data");
        return false;
    }
    
    // 1Î∞îÏù¥???ΩÍ∏∞??Î°úÍ∑∏ Ï∂úÎ†•?òÏ? ?äÏùå (?àÎ¨¥ ÎßéÏ? Î°úÍ∑∏ Î∞©Ï?)
    // ?¨Îü¨ Î∞îÏù¥???ΩÏùÑ ?åÎßå Î°úÍ∑∏ Ï∂úÎ†•
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
    // Íµ¨ÌòÑ ?ùÎûµ (?ÑÏöî??Ï∂îÍ?)
    return true;
}

bool SerialPort::setStopBits(uint8_t stopBits) {
    // Íµ¨ÌòÑ ?ùÎûµ (?ÑÏöî??Ï∂îÍ?)
    return true;
}

bool SerialPort::setParity(uint8_t parity) {
    // Íµ¨ÌòÑ ?ùÎûµ (?ÑÏöî??Ï∂îÍ?)
    return true;
}

bool SerialPort::configurePort() {
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    if (!GetCommState(static_cast<HANDLE>(handle_), &dcb)) {
        logError("Failed to get comm state");
        return false;
    }
    
    // Í∏∞Î≥∏ ?§Ï†ï
    dcb.BaudRate = baudRate_;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
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
    
    // Î≤ÑÌçº ?¨Í∏∞ ?§Ï†ï
    SetupComm(static_cast<HANDLE>(handle_), 4096, 4096);
    
    // ?Ä?ÑÏïÑ???§Ï†ï
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

std::vector<std::string> SerialPort::getAvailablePorts() {
    std::vector<std::string> ports;
    
    // ?àÏ??§Ìä∏Î¶¨Ïóê??COM ?¨Ìä∏ Î™©Î°ù ?ΩÍ∏∞
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
                // COM ?¨Ìä∏ ?¥Î¶ÑÎß?Ï∂îÏ∂ú (?? "COM3")
                if (portName.find("COM") == 0) {
                    ports.push_back(portName);
                }
            }
            
            index++;
        }
        
        RegCloseKey(hKey);
    }
    
    // ?àÏ??§Ìä∏Î¶¨Ïóê??Ï∞æÏ? Î™ªÌïú Í≤ΩÏö∞, COM1~COM20ÍπåÏ? ?úÎèÑ
    if (ports.empty()) {
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
    // Windows APIÎ•??¨Ïö©?òÏó¨ ?åÏùº ?∞Í∏∞ (fstream ?Ä??
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
    // Windows APIÎ•??¨Ïö©?òÏó¨ ?åÏùº ?ΩÍ∏∞ (fstream ?Ä??
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
    
    // Í≥µÎ∞± ?úÍ±∞
    portName.erase(portName.find_last_not_of(" \t\n\r\f\v") + 1);
    
    if (!portName.empty()) {
        logging::Logger::getInstance().info("Loaded saved port: " + portName);
    }
    
    return portName;
}

} // namespace smartro
