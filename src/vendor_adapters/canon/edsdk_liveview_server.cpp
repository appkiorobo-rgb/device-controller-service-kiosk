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
    std::lock_guard<std::mutex> lock(frameMutex_);
    frame_.assign(data, data + len);
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

                while (running_) {
                    std::vector<uint8_t> copy;
                    {
                        std::lock_guard<std::mutex> lock(frameMutex_);
                        if (frame_.empty()) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(33));
                            continue;
                        }
                        copy = frame_;
                    }
                    char partHeader[128];
                    int partLen = snprintf(partHeader, sizeof(partHeader),
                        "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
                        copy.size());
                    if (send(client, partHeader, partLen, 0) <= 0) break;
                    if (send(client, reinterpret_cast<const char*>(copy.data()), static_cast<int>(copy.size()), 0) <= 0) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(33));
                }
            }
        }
        closesocket(client);
    }

    closesocket(listenSocket);
    WSACleanup();
}

} // namespace canon
