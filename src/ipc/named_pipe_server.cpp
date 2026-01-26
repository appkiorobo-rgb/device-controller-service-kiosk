// src/ipc/named_pipe_server.cpp
#include "ipc/named_pipe_server.h"
#include "common/uuid_generator.h"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <algorithm>

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
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            client->active = false;
            if (client->pipeHandle != INVALID_HANDLE_VALUE) {
                CloseHandle(client->pipeHandle);
            }
        }
    }

    // Wait for all client threads
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            if (client->thread.joinable()) {
                client->thread.join();
            }
        }
        clients_.clear();
    }

    if (serverThread_.joinable()) {
        serverThread_.join();
    }
}

void NamedPipeServer::serverLoop() {
    while (running_) {
        HANDLE pipeHandle = CreateNamedPipe(
            pipeName_.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
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
            // Create client connection
            auto client = std::make_shared<ClientConnection>();
            client->pipeHandle = pipeHandle;
            client->active = true;
            client->thread = std::thread(&NamedPipeServer::clientThread, this, client);

            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(client);
        } else {
            CloseHandle(pipeHandle);
        }
    }
}

void NamedPipeServer::clientThread(std::shared_ptr<ClientConnection> client) {
    while (running_ && client->active) {
        std::string message = readMessage(client->pipeHandle);
        if (message.empty()) {
            break;
        }

        std::string response;
        handler_(message, response);

        if (!writeMessage(client->pipeHandle, response)) {
            break;
        }
    }

    // Cleanup
    if (client->pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(client->pipeHandle);
        client->pipeHandle = INVALID_HANDLE_VALUE;
    }

    removeClient(client);
}

void NamedPipeServer::handleClient(HANDLE pipeHandle) {
    // Legacy method - now handled by clientThread
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
    std::string eventJson = event.toJson().dump();
    
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    // Remove inactive clients
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [](const std::shared_ptr<ClientConnection>& client) {
                return !client->active || client->pipeHandle == INVALID_HANDLE_VALUE;
            }),
        clients_.end()
    );

    // Broadcast to all active clients
    for (auto& client : clients_) {
        if (client->active && client->pipeHandle != INVALID_HANDLE_VALUE) {
            writeMessage(client->pipeHandle, eventJson);
        }
    }
}

void NamedPipeServer::removeClient(std::shared_ptr<ClientConnection> client) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [&client](const std::shared_ptr<ClientConnection>& c) {
                return c == client;
            }),
        clients_.end()
    );
}

} // namespace device_controller::ipc
