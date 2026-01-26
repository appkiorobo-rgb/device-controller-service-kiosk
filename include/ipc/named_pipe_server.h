// include/ipc/named_pipe_server.h
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <windows.h>
#include "ipc/message_types.h"

namespace device_controller::ipc {

// Client connection info
struct ClientConnection {
    HANDLE pipeHandle;
    std::thread thread;
    std::atomic<bool> active{true};
};

// NamedPipeServer - handles Named Pipe communication
// Part of IPC Layer
class NamedPipeServer {
public:
    using MessageHandler = std::function<void(const std::string& message, std::string& response)>;

    NamedPipeServer(const std::string& pipeName, MessageHandler handler);
    ~NamedPipeServer();

    // Start server (non-blocking)
    bool start();

    // Stop server
    void stop();

    // Check if server is running
    bool isRunning() const { return running_; }

    // Send event to all connected clients
    void broadcastEvent(const Event& event);

private:
    std::string pipeName_;
    MessageHandler handler_;
    std::atomic<bool> running_{false};
    std::thread serverThread_;
    
    std::mutex clientsMutex_;
    std::vector<std::shared_ptr<ClientConnection>> clients_;

    void serverLoop();
    void handleClient(HANDLE pipeHandle);
    void clientThread(std::shared_ptr<ClientConnection> client);
    std::string readMessage(HANDLE pipeHandle);
    bool writeMessage(HANDLE pipeHandle, const std::string& message);
    void removeClient(std::shared_ptr<ClientConnection> client);
};

} // namespace device_controller::ipc
