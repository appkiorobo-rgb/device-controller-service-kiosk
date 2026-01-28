// tests/test_pipe_connection.cpp
// 간단한 Named Pipe 연결 테스트 프로그램
// 서비스가 실행 중일 때 파이프 연결이 되는지 확인

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// 파이프 이름
const std::wstring COMMAND_PIPE = L"\\\\.\\pipe\\DeviceControllerService_Command";
const std::wstring EVENT_PIPE = L"\\\\.\\pipe\\DeviceControllerService_Event";

// 파이프 연결 테스트
bool TestPipeConnection(const std::wstring& pipeName) {
    std::wcout << L"Attempting to connect to: " << pipeName << std::endl;
    
    // 최대 10회 재시도 (1초 간격)
    const int maxRetries = 10;
    for (int retry = 0; retry < maxRetries; ++retry) {
        HANDLE pipeHandle = CreateFileW(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            std::wcout << L"✓ Successfully connected to: " << pipeName << std::endl;
            
            // 파이프 모드 설정
            DWORD mode = PIPE_READMODE_MESSAGE;
            if (!SetNamedPipeHandleState(pipeHandle, &mode, nullptr, nullptr)) {
                DWORD error = GetLastError();
                std::wcerr << L"✗ Failed to set pipe mode, error: " << error << std::endl;
                CloseHandle(pipeHandle);
                return false;
            }
            
            std::wcout << L"✓ Pipe mode set successfully" << std::endl;
            
            // 간단한 테스트 메시지 전송 (Command 파이프만)
            if (pipeName.find(L"Command") != std::wstring::npos) {
                std::wcout << L"Testing message send/receive..." << std::endl;
                
                // 테스트 메시지: {"protocolVersion":"1.0","kind":"command","commandId":"test","type":"get_state_snapshot","timestampMs":1234567890,"payload":{}}
                std::string testMessage = R"({"protocolVersion":"1.0","kind":"command","commandId":"test","type":"get_state_snapshot","timestampMs":1234567890,"payload":{}})";
                
                DWORD messageSize = static_cast<DWORD>(testMessage.length());
                DWORD bytesWritten = 0;
                
                // 메시지 크기 전송
                if (WriteFile(pipeHandle, &messageSize, sizeof(messageSize), &bytesWritten, nullptr)) {
                    std::wcout << L"✓ Message size sent: " << messageSize << L" bytes" << std::endl;
                    
                    // 메시지 본문 전송
                    if (WriteFile(pipeHandle, testMessage.c_str(), messageSize, &bytesWritten, nullptr)) {
                        std::wcout << L"✓ Message body sent: " << bytesWritten << L" bytes" << std::endl;
                        
                        // 응답 읽기 (타임아웃 5초)
                        DWORD responseSize = 0;
                        DWORD bytesRead = 0;
                        
                        // 응답 크기 읽기
                        if (ReadFile(pipeHandle, &responseSize, sizeof(responseSize), &bytesRead, nullptr)) {
                            std::wcout << L"✓ Response size received: " << responseSize << L" bytes" << std::endl;
                            
                            if (responseSize > 0 && responseSize < 4096) {
                                std::vector<char> buffer(responseSize);
                                if (ReadFile(pipeHandle, buffer.data(), responseSize, &bytesRead, nullptr)) {
                                    if (bytesRead > 0) {
                                        std::string response(buffer.data(), bytesRead);
                                        std::wcout << L"✓ Response received: " << bytesRead << L" bytes" << std::endl;
                                        std::cout << "Response content: " << response << std::endl;
                                    } else {
                                        std::wcout << L"✓ Response received: 0 bytes (empty)" << std::endl;
                                    }
                                } else {
                                    DWORD error = GetLastError();
                                    std::wcerr << L"✗ Failed to read response body, error: " << error << std::endl;
                                }
                            }
                        } else {
                            DWORD error = GetLastError();
                            std::wcerr << L"✗ Failed to read response size, error: " << error << std::endl;
                        }
                    } else {
                        DWORD error = GetLastError();
                        std::wcerr << L"✗ Failed to send message body, error: " << error << std::endl;
                    }
                } else {
                    DWORD error = GetLastError();
                    std::wcerr << L"✗ Failed to send message size, error: " << error << std::endl;
                }
            }
            
            CloseHandle(pipeHandle);
            return true;
        }
        
        DWORD error = GetLastError();
        
        if (error == ERROR_PIPE_BUSY) {
            if (retry < maxRetries - 1) {
                std::wcout << L"  Pipe is busy, retrying in 1s... (attempt " << (retry + 1) << L"/" << maxRetries << L")" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
        } else if (error == ERROR_FILE_NOT_FOUND) {
            if (retry < maxRetries - 1) {
                std::wcout << L"  Pipe not found, retrying in 1s... (attempt " << (retry + 1) << L"/" << maxRetries << L")" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
        }
        
        if (retry == maxRetries - 1) {
            std::wcerr << L"✗ Failed to connect to: " << pipeName << L", error: " << error << L" (after " << maxRetries << L" attempts)" << std::endl;
        }
    }
    
    return false;
}

int main() {
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Named Pipe Connection Test" << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << std::endl;
    
    std::wcout << L"Make sure the Device Controller Service is running!" << std::endl;
    std::wcout << L"Press Enter to start testing..." << std::endl;
    std::cin.get();
    
    std::wcout << std::endl;
    std::wcout << L"Testing Command Pipe..." << std::endl;
    std::wcout << L"----------------------------------------" << std::endl;
    bool cmdOk = TestPipeConnection(COMMAND_PIPE);
    
    std::wcout << std::endl;
    std::wcout << L"Testing Event Pipe..." << std::endl;
    std::wcout << L"----------------------------------------" << std::endl;
    bool evtOk = TestPipeConnection(EVENT_PIPE);
    
    std::wcout << std::endl;
    std::wcout << L"========================================" << std::endl;
    std::wcout << L"Test Results:" << std::endl;
    std::wcout << L"  Command Pipe: " << (cmdOk ? L"✓ PASSED" : L"✗ FAILED") << std::endl;
    std::wcout << L"  Event Pipe:   " << (evtOk ? L"✓ PASSED" : L"✗ FAILED") << std::endl;
    std::wcout << L"========================================" << std::endl;
    
    if (cmdOk && evtOk) {
        std::wcout << L"\n✓ All tests passed! The pipes are working correctly." << std::endl;
        std::wcout << L"  The issue is likely in the Flutter client code." << std::endl;
        return 0;
    } else {
        std::wcout << L"\n✗ Some tests failed!" << std::endl;
        std::wcout << L"  Please check:" << std::endl;
        std::wcout << L"  1. Is the Device Controller Service running?" << std::endl;
        std::wcout << L"  2. Check the service logs for pipe creation errors" << std::endl;
        std::wcout << L"  3. Try running the service as administrator" << std::endl;
        return 1;
    }
}
