// include/ipc/named_pipe_server.h
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <windows.h>
#include "ipc/message_types.h"

namespace device_controller::ipc {

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

    void serverLoop();
    void handleClient(HANDLE pipeHandle);
    std::string readMessage(HANDLE pipeHandle);
    bool writeMessage(HANDLE pipeHandle, const std::string& message);
};

} // namespace device_controller::ipc
