// src/vendor_adapters/canon/edsdk_liveview_server.cpp
#include "vendor_adapters/canon/edsdk_liveview_server.h"
#include "logging/logger.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace canon {

EdsdkLiveviewServer::EdsdkLiveviewServer() = default;

EdsdkLiveviewServer::~EdsdkLiveviewServer() {
    stop();
}

void EdsdkLiveviewServer::setFrame(const uint8_t* data, size_t len) {
    if (!data || len == 0) return;
    // EVF JPEG max: some cameras send full 1MB buffer (matches EdsCreateMemoryStream(1MB)).
    static constexpr size_t kMaxFrameSize = 1536 * 1024;  // 1.5MB safety margin
    if (len > kMaxFrameSize) return;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        frame_.assign(data, data + len);
    }
    // Notify after mutex release -> server thread wakes immediately to send frame.
    frameCondition_.notify_one();
}

bool EdsdkLiveviewServer::start(int port) {
    if (running_) return true;
    port_ = port;
    running_ = true;
    thread_ = std::thread(&EdsdkLiveviewServer::run, this);
    logging::Logger::getInstance().info("LiveView MJPEG server started: " + getUrl());
    return true;
}

void EdsdkLiveviewServer::stop() {
    if (!running_) return;
    running_ = false;
    frameCondition_.notify_all();   // Wake server thread if waiting, so it can exit
    if (thread_.joinable()) {
        thread_.join();
    }
    logging::Logger::getInstance().info("LiveView MJPEG server stopped");
}

void EdsdkLiveviewServer::run() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        logging::Logger::getInstance().error("LiveView: WSAStartup failed");
        return;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(static_cast<u_short>(port_));

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }
    if (listen(listenSocket, 1) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listenSocket, &fds);
        timeval tv{1, 0};
        if (select(0, &fds, nullptr, nullptr, &tv) <= 0) continue;

        SOCKET client = accept(listenSocket, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        char buf[1024];
        int n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            if (std::strstr(buf, "GET ") != nullptr) {
                const char* headers =
                    "HTTP/1.0 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=--frame\r\n"
                    "Cache-Control: no-cache\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                send(client, headers, static_cast<int>(std::strlen(headers)), 0);

                bool firstFrameSent = false;
                auto lastLogTime = std::chrono::steady_clock::now();
                while (running_) {
                    std::vector<uint8_t> copy;
                    {
                        // condition_variable::wait_for: releases mutex while waiting, wakes on setFrame() notify.
                        // Previous lock_guard + sleep_for(16ms) held mutex during sleep,
                        // blocking setFrame() for up to 16ms -- frame production bottleneck.
                        std::unique_lock<std::mutex> lock(frameMutex_);
                        if (frame_.empty()) {
                            auto now = std::chrono::steady_clock::now();
                            if (!firstFrameSent && std::chrono::duration_cast<std::chrono::seconds>(now - lastLogTime).count() >= 3) {
                                logging::Logger::getInstance().info("LiveView: waiting for first EVF frame from camera...");
                                lastLogTime = now;
                            }
                            // Wait up to 16ms with mutex released. Wakes immediately on new frame.
                            frameCondition_.wait_for(lock, std::chrono::milliseconds(16),
                                [this]{ return !frame_.empty() || !running_; });
                            if (!running_) break;
                            if (frame_.empty()) continue;
                        }
                        copy.swap(frame_);
                    }
                    char partHeader[128];
                    int partLen = snprintf(partHeader, sizeof(partHeader),
                        "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
                        copy.size());
                    if (send(client, partHeader, partLen, 0) <= 0) break;
                    if (send(client, reinterpret_cast<const char*>(copy.data()), static_cast<int>(copy.size()), 0) <= 0) break;
                    if (!firstFrameSent) {
                        logging::Logger::getInstance().info("LiveView: first frame sent to client (" + std::to_string(copy.size()) + " bytes)");
                        firstFrameSent = true;
                    }
                    // No sleep after send: next frame sent as soon as available (TCP backpressure limits rate).
                }
            }
        }
        closesocket(client);
    }

    closesocket(listenSocket);
    WSACleanup();
}

} // namespace canon
