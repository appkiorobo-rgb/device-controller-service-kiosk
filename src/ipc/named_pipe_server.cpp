// src/ipc/named_pipe_server.cpp
// logger.h? ?? include?? Windows SDK ?? ??
#include "logging/logger.h"
#include "ipc/named_pipe_server.h"

// Windows ??? ???? include (?? ?? ?? ???)
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#endif

#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>

namespace ipc {

// PipeClient implementation
PipeClient::PipeClient(HANDLE pipeHandle)
    : pipeHandle_(pipeHandle)
    , connected_(true) {
}

PipeClient::~PipeClient() {
    disconnect();
}

bool PipeClient::isConnected() const {
    return connected_ && pipeHandle_ != INVALID_HANDLE_VALUE;
}

bool PipeClient::sendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected() || pipeHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytesWritten = 0;
    DWORD messageSize = static_cast<DWORD>(message.length());
    
    // Send message size first (4 bytes, little-endian)
    if (!WriteFile(pipeHandle_, &messageSize, sizeof(messageSize), &bytesWritten, nullptr)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            connected_ = false;
        }
        logging::Logger::getInstance().error("Failed to write message size to pipe: " + std::to_string(error));
        return false;
    }
    
    // Send message body
    if (messageSize > 0) {
        if (!WriteFile(pipeHandle_, message.c_str(), messageSize, &bytesWritten, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                connected_ = false;
            }
            logging::Logger::getInstance().error("Failed to write message to pipe: " + std::to_string(error));
            return false;
        }
    }
    
    return true;
}

bool PipeClient::receiveMessage(std::string& message, uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isConnected() || pipeHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Read message size (4 bytes)
    DWORD messageSize = 0;
    DWORD bytesRead = 0;
    if (!ReadFile(pipeHandle_, &messageSize, sizeof(messageSize), &bytesRead, nullptr)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE) {
            connected_ = false;
            pipeHandle_ = INVALID_HANDLE_VALUE;
        }
        return false;
    }
    
    if (messageSize == 0 || messageSize > BUFFER_SIZE) {
        logging::Logger::getInstance().warn("Invalid message size: " + std::to_string(messageSize));
        return false;
    }
    
    // Read message body
    std::vector<char> buffer(messageSize);
    if (!ReadFile(pipeHandle_, buffer.data(), messageSize, &bytesRead, nullptr)) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE) {
            connected_ = false;
            pipeHandle_ = INVALID_HANDLE_VALUE;
        }
        return false;
    }
    
    message.assign(buffer.data(), messageSize);
    return true;
}

void PipeClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pipeHandle_ != INVALID_HANDLE_VALUE) {
        // For client-side: just close the handle
        // DisconnectNamedPipe should be called by the server side
        FlushFileBuffers(pipeHandle_);
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
    }
    
    connected_ = false;
}

void PipeClient::disconnectServerSide() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (pipeHandle_ != INVALID_HANDLE_VALUE) {
        // Server-side disconnect: call DisconnectNamedPipe first
        FlushFileBuffers(pipeHandle_);
        DisconnectNamedPipe(pipeHandle_);
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
    }
    
    connected_ = false;
}

// NamedPipeServer implementation
NamedPipeServer::NamedPipeServer(const std::string& pipeName)
    : pipeName_(pipeName)
    , running_(false)
    , pipeCreated_(false)
    , clientDisconnectedCallback_(nullptr)
    , clientConnectedCallback_(nullptr) {
}

NamedPipeServer::~NamedPipeServer() {
    stop();
}

bool NamedPipeServer::start(MessageHandler handler) {
    if (running_) {
        return true;
    }
    
    messageHandler_ = handler;
    running_ = true;
    serverThread_ = std::thread(&NamedPipeServer::serverThread, this);
    
    logging::Logger::getInstance().info("Named Pipe server thread started: " + pipeName_);
    return true;
}

void NamedPipeServer::setClientDisconnectedCallback(ClientDisconnectedCallback callback) {
    clientDisconnectedCallback_ = callback;
}

void NamedPipeServer::setClientConnectedCallback(ClientConnectedCallback callback) {
    clientConnectedCallback_ = callback;
}

void NamedPipeServer::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // Disconnect all clients (server-side)
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        for (auto& client : clients_) {
            if (client) {
                client->disconnectServerSide();
            }
        }
        clients_.clear();
    }
    
    // Wait for server thread to finish
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    logging::Logger::getInstance().info("Named Pipe server stopped");
}

void NamedPipeServer::serverThread() {
    logging::Logger::getInstance().info("Named Pipe server thread started");
    logging::Logger::getInstance().info("Pipe name: " + pipeName_);
    
    std::wstring widePipeName(pipeName_.begin(), pipeName_.end());
    
    // Create security attributes once
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;
    
    SECURITY_DESCRIPTOR sd = {};
    if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
        DWORD error = GetLastError();
        logging::Logger::getInstance().error("Failed to initialize security descriptor: " + std::to_string(error));
        return;
    }
    
    if (!SetSecurityDescriptorDacl(&sd, TRUE, nullptr, FALSE)) {
        DWORD error = GetLastError();
        logging::Logger::getInstance().error("Failed to set security descriptor DACL: " + std::to_string(error));
        return;
    }
    
    sa.lpSecurityDescriptor = &sd;
    
    // Create named pipe once (reuse for all clients)
    HANDLE pipeHandle = CreateNamedPipeW(
        widePipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        MAX_INSTANCES,
        BUFFER_SIZE,
        BUFFER_SIZE,
        PIPE_TIMEOUT_MS,
        &sa
    );
    
    if (pipeHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        logging::Logger::getInstance().error("Failed to create named pipe: " + std::to_string(error));
        std::cout << "ERROR: Failed to create named pipe. Error code: " << error << std::endl;
        return;
    }
    
    pipeCreated_ = true;
    logging::Logger::getInstance().info("Named pipe created successfully: " + pipeName_);
    std::cout << "Named pipe created: " << pipeName_ << std::endl;
    std::cout << "Waiting for client connections..." << std::endl;
    
    // Keep the pipe open and accept multiple clients
    while (running_) {
        // Wait for client connection
        OVERLAPPED overlapped = {};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        
        bool connected = ConnectNamedPipe(pipeHandle, &overlapped);
        DWORD error = GetLastError();
        
        if (!connected && error == ERROR_IO_PENDING) {
            // Wait for connection
            DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 1000);  // 1 second timeout to check running_
            if (waitResult == WAIT_OBJECT_0) {
                DWORD bytesTransferred = 0;
                if (GetOverlappedResult(pipeHandle, &overlapped, &bytesTransferred, FALSE)) {
                    connected = true;
                } else {
                    DWORD overlappedError = GetLastError();
                    if (overlappedError != ERROR_PIPE_CONNECTED) {
                        logging::Logger::getInstance().error("GetOverlappedResult failed: " + std::to_string(overlappedError));
                    }
                }
            } else if (waitResult == WAIT_TIMEOUT) {
                // Timeout - check if still running and retry
                CloseHandle(overlapped.hEvent);
                continue;
            }
        } else if (error == ERROR_PIPE_CONNECTED) {
            // Client already connected before ConnectNamedPipe was called
            connected = true;
            logging::Logger::getInstance().info("Client connected before ConnectNamedPipe call");
        } else if (connected) {
            // ConnectNamedPipe returned TRUE immediately
            logging::Logger::getInstance().info("ConnectNamedPipe returned TRUE immediately");
        } else {
            logging::Logger::getInstance().error("ConnectNamedPipe failed with error: " + std::to_string(error));
        }
        
        CloseHandle(overlapped.hEvent);
        
        if (!connected) {
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        
        logging::Logger::getInstance().info("Client connected to named pipe");
        std::cout << "Client connected to named pipe!" << std::endl;
        
        // Notify client connected callback (for status check)
        if (clientConnectedCallback_) {
            try {
                clientConnectedCallback_();
            } catch (const std::exception& e) {
                logging::Logger::getInstance().error("Error in client connected callback: " + std::string(e.what()));
            }
        }
        
        // Create client object with the pipe handle
        // Note: MAX_INSTANCES is 1, so only one client at a time
        auto client = std::make_shared<PipeClient>(pipeHandle);
        
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            clients_.push_back(client);
        }
        
        // Start client thread and wait for it to finish
        // This keeps the pipe open and reuses it for the next client
        std::thread clientThread(&NamedPipeServer::clientThread, this, client);
        clientThread.join();  // Wait for client to disconnect
        
        // After client disconnects, disconnect the pipe connection
        // and loop back to ConnectNamedPipe to wait for the next client
        // The pipe handle stays open, we just disconnect the client connection
        DisconnectNamedPipe(pipeHandle);
        logging::Logger::getInstance().info("Pipe disconnected, ready for next client");
    }
    
    // Close pipe when server stops
    if (pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeHandle);
    }
    
    logging::Logger::getInstance().info("Named Pipe server thread exiting");
}

void NamedPipeServer::clientThread(std::shared_ptr<PipeClient> client) {
    logging::Logger::getInstance().info("Client thread started - connection will be kept alive");
    
    while (running_ && client->isConnected()) {
        std::string message;
        
        // Receive message with timeout - this keeps the connection alive
        // Timeout is normal - it allows us to check running_ periodically
        if (client->receiveMessage(message, 1000)) {
            if (!message.empty() && messageHandler_) {
                try {
                    logging::Logger::getInstance().debug("Received message from client, processing...");
                    messageHandler_(*client, message);
                    logging::Logger::getInstance().debug("Message processed successfully");
                } catch (const std::exception& e) {
                    logging::Logger::getInstance().error("Error in message handler: " + std::string(e.what()));
                }
            }
        } else {
            // Timeout or connection lost
            if (!client->isConnected()) {
                logging::Logger::getInstance().info("Client connection lost");
                break;
            }
            // Timeout is normal - continue loop to keep connection alive
        }
    }
    
    // Client disconnected - cancel any ongoing payment operations
    logging::Logger::getInstance().info("Client thread ending - client disconnected or server stopping");
    
    // Notify IPC server about client disconnection for cleanup
    if (clientDisconnectedCallback_) {
        try {
            clientDisconnectedCallback_();
        } catch (const std::exception& e) {
            logging::Logger::getInstance().error("Error in client disconnected callback: " + std::string(e.what()));
        }
    }
    
    client->disconnect();  // Just close the handle, server will call DisconnectNamedPipe
    
    // Remove client from list
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                [client](const std::shared_ptr<PipeClient>& c) {
                    return c == client;
                }),
            clients_.end()
        );
    }
    
    logging::Logger::getInstance().debug("Client thread exiting");
}

bool NamedPipeServer::sendToClient(PipeClient& client, const std::string& message) {
    return client.sendMessage(message);
}

void NamedPipeServer::broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    // Remove disconnected clients
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [](const std::shared_ptr<PipeClient>& client) {
                return !client->isConnected();
            }),
        clients_.end()
    );
    
    logging::Logger::getInstance().debug("Broadcasting message to " + std::to_string(clients_.size()) + " client(s)");
    
    // Broadcast to all connected clients
    size_t sentCount = 0;
    for (auto& client : clients_) {
        if (client->isConnected()) {
            if (client->sendMessage(message)) {
                sentCount++;
            } else {
                logging::Logger::getInstance().warn("Failed to send broadcast message to client");
            }
        }
    }
    
    logging::Logger::getInstance().debug("Broadcast message sent to " + std::to_string(sentCount) + " client(s)");
}

size_t NamedPipeServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return std::count_if(clients_.begin(), clients_.end(),
        [](const std::shared_ptr<PipeClient>& client) {
            return client->isConnected();
        });
}

} // namespace ipc
