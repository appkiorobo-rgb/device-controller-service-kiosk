// tests/test_pipe_client.cpp
// Simple Named Pipe client test program

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

int main() {
    const wchar_t* pipeName = L"\\\\.\\pipe\\DeviceControllerService";
    
    std::wcout << L"Attempting to connect to pipe: " << pipeName << std::endl;
    
    // Try to connect to named pipe
    HANDLE pipeHandle = CreateFileW(
        pipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,  // Not using overlapped I/O for simplicity
        nullptr
    );
    
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::wcout << L"Failed to connect. Error code: " << error << std::endl;
        
        if (error == ERROR_FILE_NOT_FOUND) {
            std::wcout << L"ERROR_FILE_NOT_FOUND (2): Pipe does not exist" << std::endl;
            std::wcout << L"Make sure the service is running and has created the pipe." << std::endl;
        } else if (error == ERROR_PIPE_BUSY) {
            std::wcout << L"ERROR_PIPE_BUSY (231): Pipe is busy" << std::endl;
            std::wcout << L"Waiting for pipe to become available..." << std::endl;
            
            if (WaitNamedPipeW(pipeName, 5000)) {
                pipeHandle = CreateFileW(
                    pipeName,
                    GENERIC_READ | GENERIC_WRITE,
                    0,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr
                );
            }
        } else {
            std::wcout << L"Unknown error: " << error << std::endl;
        }
    }
    
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        std::wcout << L"Connection failed!" << std::endl;
        return 1;
    }
    
    std::wcout << L"Successfully connected to pipe!" << std::endl;
    
    // Set pipe mode
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipeHandle, &mode, nullptr, nullptr)) {
        DWORD error = GetLastError();
        std::wcout << L"Failed to set pipe mode. Error: " << error << std::endl;
        CloseHandle(pipeHandle);
        return 1;
    }
    
    std::wcout << L"Pipe mode set to PIPE_READMODE_MESSAGE" << std::endl;
    
    // Send a test message
    std::string testMessage = R"({"protocolVersion":"1.0","kind":"command","commandId":"test-123","type":"get_state_snapshot","timestampMs":1234567890,"payload":{}})";
    
    DWORD messageSize = static_cast<DWORD>(testMessage.length());
    DWORD bytesWritten = 0;
    
    std::cout << "Sending message: " << testMessage << std::endl;
    
    // Send message size first
    if (!WriteFile(pipeHandle, &messageSize, sizeof(messageSize), &bytesWritten, nullptr)) {
        DWORD error = GetLastError();
        std::wcout << L"Failed to write message size. Error: " << error << std::endl;
        CloseHandle(pipeHandle);
        return 1;
    }
    
    // Send message body
    if (!WriteFile(pipeHandle, testMessage.c_str(), messageSize, &bytesWritten, nullptr)) {
        DWORD error = GetLastError();
        std::wcout << L"Failed to write message. Error: " << error << std::endl;
        CloseHandle(pipeHandle);
        return 1;
    }
    
    std::cout << "Message sent successfully (" << bytesWritten << " bytes)" << std::endl;
    
    // Read response
    DWORD responseSize = 0;
    DWORD bytesRead = 0;
    
    std::cout << "Waiting for response..." << std::endl;
    
    // Read response size
    if (!ReadFile(pipeHandle, &responseSize, sizeof(responseSize), &bytesRead, nullptr)) {
        DWORD error = GetLastError();
        std::wcout << L"Failed to read response size. Error: " << error << std::endl;
        CloseHandle(pipeHandle);
        return 1;
    }
    
    if (responseSize == 0 || responseSize > 4096) {
        std::wcout << L"Invalid response size: " << responseSize << std::endl;
        CloseHandle(pipeHandle);
        return 1;
    }
    
    // Read response body
    std::vector<char> buffer(responseSize);
    if (!ReadFile(pipeHandle, buffer.data(), responseSize, &bytesRead, nullptr)) {
        DWORD error = GetLastError();
        std::wcout << L"Failed to read response. Error: " << error << std::endl;
        CloseHandle(pipeHandle);
        return 1;
    }
    
    std::string response(buffer.data(), bytesRead);
    std::cout << "Response received (" << bytesRead << " bytes):" << std::endl;
    std::cout << response << std::endl;
    
    // Close pipe
    CloseHandle(pipeHandle);
    std::wcout << L"Connection closed." << std::endl;
    
    return 0;
}
