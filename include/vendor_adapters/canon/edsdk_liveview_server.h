// include/vendor_adapters/canon/edsdk_liveview_server.h
#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#include <cstdint>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>

namespace canon {

/// Serves EDSDK EVF frames as MJPEG over HTTP (GET /liveview).
/// Used so Flutter can display LiveView via a single URL.
class EdsdkLiveviewServer {
public:
    static constexpr int DEFAULT_PORT = 8081;
    static constexpr const char* DEFAULT_PATH = "/liveview";

    EdsdkLiveviewServer();
    ~EdsdkLiveviewServer();

    /// Set latest JPEG frame (called from command processor thread).
    void setFrame(const uint8_t* data, size_t len);

    /// Start HTTP server on port. Returns true if started.
    bool start(int port = DEFAULT_PORT);
    /// Stop server and join thread.
    void stop();

    bool isRunning() const { return running_; }
    std::string getUrl() const { return "http://127.0.0.1:" + std::to_string(port_) + DEFAULT_PATH; }
    int getPort() const { return port_; }

private:
    void run();

    std::vector<uint8_t> frame_;
    std::mutex frameMutex_;
    std::atomic<bool> running_{false};
    int port_{DEFAULT_PORT};
    std::thread thread_;
};

} // namespace canon
