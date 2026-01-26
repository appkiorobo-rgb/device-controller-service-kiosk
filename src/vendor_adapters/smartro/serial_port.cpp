// src/vendor_adapters/smartro/serial_port.cpp
#include "vendor_adapters/smartro/serial_port.h"
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <chrono>

#pragma comment(lib, "setupapi.lib")

namespace device_controller::vendor::smartro {

SerialPort::SerialPort() : handle_(INVALID_HANDLE_VALUE), baudRate_(115200) {
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
    std::cout << "[SERIAL] Opening port: " << fullPortName << " (baud: " << baudRate << ")" << std::endl;
    
    // FILE_FLAG_OVERLAPPED 없이 동기 모드로 열기 (타임아웃이 제대로 작동하도록)
    handle_ = CreateFile(
        fullPortName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,  // FILE_FLAG_OVERLAPPED 없음 = 동기 모드
        nullptr
    );

    if (handle_ == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "[SERIAL] ERROR: Failed to open port " << portName << " (Error: " << error << ")" << std::endl;
        return false;
    }
    
    std::cout << "[SERIAL] Port opened, configuring..." << std::endl;
    bool configured = configurePort(baudRate);
    if (!configured) {
        std::cerr << "[SERIAL] ERROR: Failed to configure port " << portName << std::endl;
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }
    
    std::cout << "[SERIAL] Port " << portName << " opened and configured successfully" << std::endl;
    return true;
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
    
    // 중요: 흐름 제어 완전히 비활성화 (하드웨어/소프트웨어 모두)
    dcb.fOutxCtsFlow = FALSE;  // CTS 흐름 제어 비활성화
    dcb.fOutxDsrFlow = FALSE;  // DSR 흐름 제어 비활성화
    dcb.fDtrControl = DTR_CONTROL_DISABLE;  // DTR 비활성화
    dcb.fRtsControl = RTS_CONTROL_DISABLE;  // RTS 비활성화
    dcb.fOutX = FALSE;  // XON/XOFF 출력 비활성화
    dcb.fInX = FALSE;   // XON/XOFF 입력 비활성화

    if (!SetCommState(handle_, &dcb)) {
        return false;
    }
    
    // 버퍼 비우기
    PurgeComm(handle_, PURGE_TXCLEAR | PURGE_RXCLEAR);

    // Set timeouts - Write는 즉시 완료되어야 함
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    // Write timeout: 매우 짧게 설정 (쓰기는 즉시 완료되어야 함)
    timeouts.WriteTotalTimeoutConstant = 10;  // 10ms만 기다림
    timeouts.WriteTotalTimeoutMultiplier = 0;   // 바이트당 추가 시간 없음

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

    // 출력 버퍼 비우기 (이전 데이터 제거)
    PurgeComm(handle_, PURGE_TXCLEAR);
    
    // WriteFile 전 출력 버퍼 상태 확인
    COMSTAT comStatBefore;
    DWORD errorsBefore;
    if (ClearCommError(handle_, &errorsBefore, &comStatBefore)) {
        if (comStatBefore.cbOutQue > 0) {
            std::cout << "[SERIAL] Warning: Output queue has " << comStatBefore.cbOutQue << " bytes before write" << std::endl;
        }
    }
    
    // 타임아웃 재설정 (매번 확인)
    COMMTIMEOUTS timeouts = {0};
    GetCommTimeouts(handle_, &timeouts);
    timeouts.WriteTotalTimeoutConstant = 10;  // 10ms
    timeouts.WriteTotalTimeoutMultiplier = 0;
    SetCommTimeouts(handle_, &timeouts);
    
    DWORD bytesWritten = 0;
    auto writeStartTime = std::chrono::steady_clock::now();
    
    // WriteFile 호출
    BOOL result = WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()), &bytesWritten, nullptr);
    
    auto writeElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - writeStartTime).count();
    
    if (!result) {
        DWORD error = GetLastError();
        std::cerr << "[SERIAL] WriteFile failed (Error: " << error << ", elapsed: " << writeElapsed << "ms)" << std::endl;
        return -1;
    }
    
    if (writeElapsed > 10) {
        std::cout << "[SERIAL] WriteFile took " << writeElapsed << "ms for " << bytesWritten << " bytes (expected <10ms)" << std::endl;
    }
    
    // WriteFile 후 출력 버퍼 상태 확인
    COMSTAT comStatAfter;
    DWORD errorsAfter;
    if (ClearCommError(handle_, &errorsAfter, &comStatAfter)) {
        if (comStatAfter.cbOutQue > 0) {
            std::cout << "[SERIAL] Output queue after write: " << comStatAfter.cbOutQue << " bytes remaining" << std::endl;
        }
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
        if (testPort.open(portName, 115200)) {
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
