// include/ipc/ipc_server.h
#pragma once

#include "ipc/named_pipe_server.h"
#include "ipc/message_types.h"
#include "ipc/message_parser.h"
#include "core/device_manager.h"
#include <string>
#include <memory>
#include <functional>
#include <map>

namespace ipc {

// IPC Server (Named Pipe based)
class IpcServer {
public:
    using CommandHandler = std::function<Response(const Command&)>;
    
    IpcServer(core::DeviceManager& deviceManager);
    ~IpcServer();
    
    // Start server
    bool start();
    
    // Stop server
    void stop();
    
    // Register command handler
    void registerHandler(CommandType type, CommandHandler handler);
    
    // Broadcast event to all connected clients
    void broadcastEvent(const Event& event);
    
    bool isRunning() const { return pipeServer_ && pipeServer_->isRunning(); }
    
    NamedPipeServer& getPipeServer() { return *pipeServer_; }
    
private:
    void handlePipeMessage(PipeClient& client, const std::string& message);
    Response processCommand(const Command& command);
    
    std::unique_ptr<NamedPipeServer> pipeServer_;
    core::DeviceManager& deviceManager_;
    std::map<CommandType, CommandHandler> commandHandlers_;
    
    static constexpr const char* PIPE_NAME = "\\\\.\\pipe\\DeviceControllerService";
};

} // namespace ipc
