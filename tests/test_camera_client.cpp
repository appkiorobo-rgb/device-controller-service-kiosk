// tests/test_camera_client.cpp
// Camera test client for Named Pipe IPC

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <map>
#include <random>

class NamedPipeClient {
public:
    NamedPipeClient(const wchar_t* pipeName) : pipeName_(pipeName), pipeHandle_(INVALID_HANDLE_VALUE) {}
    
    ~NamedPipeClient() {
        disconnect();
    }
    
    bool connect() {
        std::wcout << L"Connecting to pipe: " << pipeName_ << std::endl;
        
        pipeHandle_ = CreateFileW(
            pipeName_,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        
        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_BUSY) {
                std::wcout << L"Pipe is busy, waiting..." << std::endl;
                if (WaitNamedPipeW(pipeName_, 5000)) {
                    pipeHandle_ = CreateFileW(
                        pipeName_,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        nullptr,
                        OPEN_EXISTING,
                        0,
                        nullptr
                    );
                }
            }
            
            if (pipeHandle_ == INVALID_HANDLE_VALUE) {
                std::wcout << L"Failed to connect. Error: " << GetLastError() << std::endl;
                return false;
            }
        }
        
        // Set pipe mode
        DWORD mode = PIPE_READMODE_MESSAGE;
        if (!SetNamedPipeHandleState(pipeHandle_, &mode, nullptr, nullptr)) {
            std::wcout << L"Failed to set pipe mode. Error: " << GetLastError() << std::endl;
            CloseHandle(pipeHandle_);
            pipeHandle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        
        std::wcout << L"Connected successfully!" << std::endl;
        return true;
    }
    
    void disconnect() {
        if (pipeHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle_);
            pipeHandle_ = INVALID_HANDLE_VALUE;
        }
    }
    
    bool sendMessage(const std::string& message) {
        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD messageSize = static_cast<DWORD>(message.length());
        DWORD bytesWritten = 0;
        
        // Send message size
        if (!WriteFile(pipeHandle_, &messageSize, sizeof(messageSize), &bytesWritten, nullptr)) {
            std::wcout << L"Failed to write message size. Error: " << GetLastError() << std::endl;
            return false;
        }
        
        // Send message body
        if (!WriteFile(pipeHandle_, message.c_str(), messageSize, &bytesWritten, nullptr)) {
            std::wcout << L"Failed to write message. Error: " << GetLastError() << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool receiveMessage(std::string& message) {
        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            return false;
        }
        
        DWORD responseSize = 0;
        DWORD bytesRead = 0;
        
        // Read response size
        if (!ReadFile(pipeHandle_, &responseSize, sizeof(responseSize), &bytesRead, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_MORE_DATA || error == ERROR_IO_PENDING) {
                // Try to read anyway
            } else {
                return false;
            }
        }
        
        if (responseSize == 0 || responseSize > 65536) {
            return false;
        }
        
        // Read response body
        std::vector<char> buffer(responseSize);
        if (!ReadFile(pipeHandle_, buffer.data(), responseSize, &bytesRead, nullptr)) {
            return false;
        }
        
        message.assign(buffer.data(), bytesRead);
        return true;
    }
    
    bool isConnected() const {
        return pipeHandle_ != INVALID_HANDLE_VALUE;
    }
    
private:
    const wchar_t* pipeName_;
    HANDLE pipeHandle_;
};

std::string createCommand(const std::string& commandType, const std::string& commandId, const std::map<std::string, std::string>& payload = {}) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::ostringstream json;
    json << R"({"protocolVersion":"1.0","kind":"command","commandId":")" << commandId
         << R"(","type":")" << commandType
         << R"(","timestampMs":)" << timestamp
         << R"(,"payload":{)";
    
    bool first = true;
    for (const auto& p : payload) {
        if (!first) json << ",";
        json << "\"" << p.first << "\":\"" << p.second << "\"";
        first = false;
    }
    
    json << "}}";
    return json.str();
}

std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 8; ++i) {
        oss << dis(gen);
    }
    return oss.str();
}

void printResponse(const std::string& response) {
    std::cout << "Response: " << response << std::endl;
}

int main() {
    const wchar_t* pipeName = L"\\\\.\\pipe\\DeviceControllerService";
    
    std::wcout << L"=== Camera Test Client ===" << std::endl;
    std::wcout << L"Connecting to service..." << std::endl;
    
    NamedPipeClient client(pipeName);
    
    if (!client.connect()) {
        std::wcout << L"Failed to connect to service. Make sure the service is running." << std::endl;
        return 1;
    }
    
    // Test 1: Get camera status
    std::cout << "\n=== Test 1: Get Camera Status ===" << std::endl;
    std::string cmdId1 = generateUUID();
    std::string cmd1 = createCommand("camera_status", cmdId1);
    std::cout << "Sending: " << cmd1 << std::endl;
    
    if (client.sendMessage(cmd1)) {
        std::string response1;
        if (client.receiveMessage(response1)) {
            printResponse(response1);
        } else {
            std::cout << "Failed to receive response" << std::endl;
        }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Test 2: Capture photo
    std::cout << "\n=== Test 2: Capture Photo ===" << std::endl;
    std::string captureId = "test_capture_" + generateUUID();
    std::string cmdId2 = generateUUID();
    std::map<std::string, std::string> payload;
    payload["captureId"] = captureId;
    std::string cmd2 = createCommand("camera_capture", cmdId2, payload);
    std::cout << "Sending: " << cmd2 << std::endl;
    std::cout << "Capture ID: " << captureId << std::endl;
    
    if (client.sendMessage(cmd2)) {
        std::string response2;
        if (client.receiveMessage(response2)) {
            printResponse(response2);
        } else {
            std::cout << "Failed to receive response" << std::endl;
        }
    }
    
    // Wait for capture complete event
    std::cout << "\n=== Waiting for capture_complete event ===" << std::endl;
    std::cout << "Waiting up to 30 seconds..." << std::endl;
    
    bool eventReceived = false;
    auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30);
    
    while (!eventReceived && (std::chrono::steady_clock::now() - startTime) < timeout) {
        std::string eventMsg;
        if (client.receiveMessage(eventMsg)) {
            if (eventMsg.find("camera_capture_complete") != std::string::npos) {
                std::cout << "\n=== Capture Complete Event Received ===" << std::endl;
                printResponse(eventMsg);
                eventReceived = true;
            } else {
                std::cout << "Received event: " << eventMsg << std::endl;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    if (!eventReceived) {
        std::cout << "Timeout: No capture_complete event received within 30 seconds" << std::endl;
    }
    
    // Test 3: Get camera status again
    std::cout << "\n=== Test 3: Get Camera Status (after capture) ===" << std::endl;
    std::string cmdId3 = generateUUID();
    std::string cmd3 = createCommand("camera_status", cmdId3);
    std::cout << "Sending: " << cmd3 << std::endl;
    
    if (client.sendMessage(cmd3)) {
        std::string response3;
        if (client.receiveMessage(response3)) {
            printResponse(response3);
        }
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();
    
    client.disconnect();
    return 0;
}
