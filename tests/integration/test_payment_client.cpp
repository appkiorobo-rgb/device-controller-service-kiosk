// tests/integration/test_payment_client.cpp
// C++ 테스트 클라이언트 (Windows Named Pipe)

#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>

constexpr const char* PIPE_NAME = "\\\\.\\pipe\\DeviceControllerService";
constexpr const char* PROTOCOL_VERSION = "1.0";

class TestClient {
public:
    TestClient() : pipeHandle_(INVALID_HANDLE_VALUE) {}
    
    ~TestClient() {
        disconnect();
    }
    
    bool connect() {
        pipeHandle_ = CreateFile(
            PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        
        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to connect to pipe. Error: " << GetLastError() << std::endl;
            return false;
        }
        
        std::cout << "Connected to " << PIPE_NAME << std::endl;
        return true;
    }
    
    void disconnect() {
        if (pipeHandle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle_);
            pipeHandle_ = INVALID_HANDLE_VALUE;
        }
    }
    
    nlohmann::json sendCommand(const std::string& type, const nlohmann::json& payload = {}) {
        if (pipeHandle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "Not connected" << std::endl;
            return {};
        }
        
        // Generate UUID (simple version)
        std::string commandId = generateUUID();
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        nlohmann::json command = {
            {"protocolVersion", PROTOCOL_VERSION},
            {"kind", "command"},
            {"commandId", commandId},
            {"type", type},
            {"timestampMs", now},
            {"payload", payload}
        };
        
        std::string message = command.dump();
        
        // Send message
        DWORD bytesWritten = 0;
        if (!WriteFile(pipeHandle_, message.c_str(), 
                      static_cast<DWORD>(message.length()), 
                      &bytesWritten, nullptr)) {
            std::cerr << "Failed to write. Error: " << GetLastError() << std::endl;
            return {};
        }
        
        // Read response
        char buffer[4096];
        DWORD bytesRead = 0;
        if (!ReadFile(pipeHandle_, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
            std::cerr << "Failed to read. Error: " << GetLastError() << std::endl;
            return {};
        }
        
        buffer[bytesRead] = '\0';
        try {
            return nlohmann::json::parse(buffer);
        } catch (...) {
            std::cerr << "Failed to parse response" << std::endl;
            return {};
        }
    }
    
    void listenEvents(int timeoutSeconds = 30) {
        std::cout << "Listening for events (timeout: " << timeoutSeconds << "s)..." << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            
            if (elapsed > timeoutSeconds) {
                std::cout << "\nTimeout after " << timeoutSeconds << " seconds" << std::endl;
                break;
            }
            
            char buffer[4096];
            DWORD bytesRead = 0;
            
            // Try to read (non-blocking would be better, but this is simple)
            if (ReadFile(pipeHandle_, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
                buffer[bytesRead] = '\0';
                try {
                    auto event = nlohmann::json::parse(buffer);
                    if (event.value("kind", "") == "event") {
                        printEvent(event);
                    }
                } catch (...) {
                    // Not JSON or parse error
                }
            } else {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    std::cout << "Connection closed" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
private:
    HANDLE pipeHandle_;
    
    std::string generateUUID() {
        // Simple UUID v4 generation
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);
        
        std::ostringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++) ss << dis(gen);
        return ss.str();
    }
    
    void printEvent(const nlohmann::json& event) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_s(&tm_buf, &time);
        
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tm_buf);
        
        std::cout << "\n[" << timeStr << "] Event: " 
                  << event.value("eventType", "unknown") << std::endl;
        std::cout << "  Device: " << event.value("deviceType", "unknown") << std::endl;
        std::cout << "  Data: " << event.value("data", nlohmann::json::object()).dump(2) << std::endl;
    }
};

void printResponse(const nlohmann::json& response) {
    if (response.empty()) {
        std::cout << "No response received" << std::endl;
        return;
    }
    
    std::cout << "\nResponse:" << std::endl;
    std::cout << "  Status: " << response.value("status", "UNKNOWN") << std::endl;
    std::cout << "  Command ID: " << response.value("commandId", "N/A") << std::endl;
    
    if (response.contains("error") && !response["error"].is_null()) {
        std::cout << "  Error Code: " << response["error"].value("code", "N/A") << std::endl;
        std::cout << "  Error Message: " << response["error"].value("message", "N/A") << std::endl;
    } else {
        std::cout << "  Result: " << response.value("result", nlohmann::json::object()).dump(2) << std::endl;
    }
}

std::string stateName(int state) {
    switch (state) {
        case 0: return "DISCONNECTED";
        case 1: return "CONNECTING";
        case 2: return "READY";
        case 3: return "PROCESSING";
        case 4: return "ERROR";
        case 5: return "HUNG";
        default: return "UNKNOWN(" + std::to_string(state) + ")";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  test_payment_client.exe status          # 상태 확인" << std::endl;
        std::cout << "  test_payment_client.exe start <amount>    # 결제 시작" << std::endl;
        std::cout << "  test_payment_client.exe cancel           # 결제 취소" << std::endl;
        std::cout << "  test_payment_client.exe reset            # 단말기 리셋" << std::endl;
        std::cout << "  test_payment_client.exe check            # 장치 체크" << std::endl;
        std::cout << "  test_payment_client.exe listen [timeout]  # 이벤트 수신" << std::endl;
        return 1;
    }
    
    TestClient client;
    if (!client.connect()) {
        return 1;
    }
    
    std::string command = argv[1];
    
    try {
        if (command == "status") {
            std::cout << "Checking payment terminal status..." << std::endl;
            auto response = client.sendCommand("payment_status_check");
            printResponse(response);
            
            if (!response.empty() && response.value("status", "") == "OK") {
                auto result = response.value("result", nlohmann::json::object());
                if (result.contains("state")) {
                    std::cout << "\nState: " << stateName(result["state"]) << std::endl;
                }
            }
        }
        else if (command == "start") {
            if (argc < 3) {
                std::cerr << "Usage: test_payment_client.exe start <amount>" << std::endl;
                return 1;
            }
            int amount = std::stoi(argv[2]);
            std::cout << "Starting payment: " << amount << "원" << std::endl;
            auto response = client.sendCommand("payment_start", {{"amount", amount}});
            printResponse(response);
            
            if (!response.empty() && response.value("status", "") == "OK") {
                auto result = response.value("result", nlohmann::json::object());
                if (result.contains("state")) {
                    std::cout << "\nState: " << stateName(result["state"]) << std::endl;
                }
                std::cout << "\n결제 결과는 이벤트로 수신됩니다. 'listen' 명령으로 확인하세요." << std::endl;
            }
        }
        else if (command == "cancel") {
            std::cout << "Cancelling payment..." << std::endl;
            auto response = client.sendCommand("payment_cancel");
            printResponse(response);
        }
        else if (command == "reset") {
            std::cout << "Resetting payment terminal..." << std::endl;
            auto response = client.sendCommand("payment_reset");
            printResponse(response);
        }
        else if (command == "check") {
            std::cout << "Checking payment device..." << std::endl;
            auto response = client.sendCommand("payment_device_check");
            printResponse(response);
        }
        else if (command == "listen") {
            int timeout = argc > 2 ? std::stoi(argv[2]) : 30;
            client.listenEvents(timeout);
        }
        else {
            std::cerr << "Unknown command: " << command << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
