// include/vendor_adapters/smartro/serial_port.h
#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Windows HANDLE 타입 (헤더 의존성 최소화)
#ifdef _WIN32
    #ifndef INVALID_HANDLE_VALUE
        #define INVALID_HANDLE_VALUE ((void*)(-1))
    #endif
#endif

namespace smartro {

class SerialPort {
public:
    SerialPort();
    ~SerialPort();
    
    // 복사 및 이동 방지
    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&&) = delete;
    SerialPort& operator=(SerialPort&&) = delete;
    
    // 포트 열기/닫기
    bool open(const std::string& portName, uint32_t baudRate = 115200);
    void close();
    bool isOpen() const { return handle_ != INVALID_HANDLE_VALUE; }
    
    // 데이터 읽기/쓰기
    bool write(const uint8_t* data, size_t length);
    bool read(uint8_t* buffer, size_t bufferSize, size_t& bytesRead, uint32_t timeoutMs = 1000);
    
    // 설정
    bool setBaudRate(uint32_t baudRate);
    bool setDataBits(uint8_t dataBits);
    bool setStopBits(uint8_t stopBits);
    bool setParity(uint8_t parity);
    
    // 포트 이름 반환
    std::string getPortName() const { return portName_; }
    
    // 사용 가능한 COM 포트 목록 가져오기
    static std::vector<std::string> getAvailablePorts();
    
    // 성공한 COM 포트 저장
    static bool saveWorkingPort(const std::string& portName);
    
    // 저장된 COM 포트 읽기
    static std::string loadWorkingPort();
    
private:
    void* handle_;  // HANDLE (void*로 선언하여 windows.h 의존성 최소화)
    std::string portName_;
    uint32_t baudRate_;
    
    bool configurePort();
    void logError(const std::string& operation);
};

} // namespace smartro
