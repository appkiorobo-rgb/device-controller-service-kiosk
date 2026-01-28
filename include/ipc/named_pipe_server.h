// include/ipc/named_pipe_server.h
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

// Windows type forward declaration (without including windows.h)
#ifdef _WIN32
    typedef void* HANDLE;
    typedef unsigned long DWORD;
    #ifndef INVALID_HANDLE_VALUE
        #define INVALID_HANDLE_VALUE ((HANDLE)(-1))
    #endif
#endif

namespace ipc {

// Named Pipe server client connection
class PipeClient {
public:
    PipeClient(HANDLE pipeHandle);
    ~PipeClient();
    
    // Prevent copy and move
    PipeClient(const PipeClient&) = delete;
    PipeClient& operator=(const PipeClient&) = delete;
    PipeClient(PipeClient&&) = delete;
    PipeClient& operator=(PipeClient&&) = delete;
    
    bool isConnected() const;
    bool sendMessage(const std::string& message);
    bool receiveMessage(std::string& message, uint32_t timeoutMs = 5000);
    void disconnect();
    
    // Server-side disconnect (calls DisconnectNamedPipe)
    void disconnectServerSide();
    
    HANDLE getHandle() const { return pipeHandle_; }
    
private:
    HANDLE pipeHandle_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;
    
    static constexpr size_t BUFFER_SIZE = 4096;
};

// Named Pipe server
class NamedPipeServer {
public:
    using MessageHandler = std::function<void(PipeClient& client, const std::string& message)>;
    using ClientDisconnectedCallback = std::function<void()>;
    using ClientConnectedCallback = std::function<void()>;
    
    NamedPipeServer(const std::string& pipeName);
    ~NamedPipeServer();
    
    // Start server with message handler
    bool start(MessageHandler handler);
    
    // Set client disconnected callback
    void setClientDisconnectedCallback(ClientDisconnectedCallback callback);
    
    // Set client connected callback
    void setClientConnectedCallback(ClientConnectedCallback callback);
    
    // Stop server
    void stop();
    
    // Send message to specific client
    bool sendToClient(PipeClient& client, const std::string& message);
    
    // Broadcast to all connected clients
    void broadcast(const std::string& message);
    
    // Get connected client count
    size_t getClientCount() const;
    
    bool isRunning() const { return running_; }
    
private:
    void serverThread();
    void clientThread(std::shared_ptr<PipeClient> client);
    
    std::string pipeName_;
    std::atomic<bool> running_;
    std::atomic<bool> pipeCreated_;
    std::thread serverThread_;
    MessageHandler messageHandler_;
    ClientDisconnectedCallback clientDisconnectedCallback_;
    ClientConnectedCallback clientConnectedCallback_;
    
    std::vector<std::shared_ptr<PipeClient>> clients_;
    mutable std::mutex clientsMutex_;
    
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr DWORD PIPE_TIMEOUT_MS = 5000;
    static constexpr DWORD MAX_INSTANCES = 1;  // Single instance for now
};

} // namespace ipc
