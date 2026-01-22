// src/ipc/named_pipe_server.cpp
#include "ipc/named_pipe_server.h"
#include <windows.h>
#include <iostream>
#include <sstream>

namespace device_controller::ipc {

NamedPipeServer::NamedPipeServer(const std::string& pipeName, MessageHandler handler)
    : pipeName_(pipeName)
    , handler_(handler)
{
}

NamedPipeServer::~NamedPipeServer() {
    stop();
}

bool NamedPipeServer::start() {
    if (running_) {
        return false;
    }

    running_ = true;
    serverThread_ = std::thread(&NamedPipeServer::serverLoop, this);
    return true;
}

void NamedPipeServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void NamedPipeServer::serverLoop() {
    while (running_) {
        HANDLE pipeHandle = CreateNamedPipe(
            pipeName_.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096,  // Output buffer size
            4096,  // Input buffer size
            0,     // Default timeout
            nullptr // Default security attributes
        );

        if (pipeHandle == INVALID_HANDLE_VALUE) {
            if (running_) {
                // Log error and retry after delay
                Sleep(1000);
            }
            continue;
        }

        // Wait for client connection
        bool connected = ConnectNamedPipe(pipeHandle, nullptr) ? true : 
                        (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected && running_) {
            handleClient(pipeHandle);
        }

        CloseHandle(pipeHandle);
    }
}

void NamedPipeServer::handleClient(HANDLE pipeHandle) {
    std::string message = readMessage(pipeHandle);
    if (message.empty()) {
        return;
    }

    std::string response;
    handler_(message, response);

    writeMessage(pipeHandle, response);
}

std::string NamedPipeServer::readMessage(HANDLE pipeHandle) {
    char buffer[4096];
    DWORD bytesRead = 0;
    
    if (!ReadFile(pipeHandle, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)) {
        return "";
    }

    buffer[bytesRead] = '\0';
    return std::string(buffer, bytesRead);
}

bool NamedPipeServer::writeMessage(HANDLE pipeHandle, const std::string& message) {
    DWORD bytesWritten = 0;
    return WriteFile(pipeHandle, message.c_str(), 
                    static_cast<DWORD>(message.length()), 
                    &bytesWritten, nullptr) != 0;
}

void NamedPipeServer::broadcastEvent(const Event& event) {
    // TODO: Implement event broadcasting to all connected clients
    // This requires maintaining a list of connected clients
}

} // namespace device_controller::ipc
