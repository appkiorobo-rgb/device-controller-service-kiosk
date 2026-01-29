// tests/test_edsdk_camera.cpp
// EDSDK-only camera test (no IPC, no device controller service).
// Use this to verify: discover camera, open session, take picture, download to host, save file.

#include "logging/logger.h"
#include "config/config_manager.h"
#include "devices/device_types.h"
#include "vendor_adapters/canon/edsdk_camera_adapter.h"

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <mutex>

static std::atomic<bool> g_captureDone{false};
static std::atomic<bool> g_captureSuccess{false};
static std::string g_captureIdReceived;
static std::mutex g_cvMutex;
static std::condition_variable g_cv;

static void onCaptureComplete(const devices::CaptureCompleteEvent& event) {
    g_captureIdReceived = event.captureId;
    g_captureSuccess = event.success;
    g_captureDone = true;
    g_cv.notify_all();
}

int main() {
    std::cout << "=== EDSDK Camera Standalone Test (no IPC) ===" << std::endl;

    logging::Logger::getInstance().initialize("logs/test_edsdk_camera.log");
    logging::Logger::getInstance().info("test_edsdk_camera started");

    config::ConfigManager::getInstance().initialize();  // void, uses defaults if no config file

    std::string deviceId = "canon_camera_001";
    auto adapter = std::make_shared<canon::EdsdkCameraAdapter>(deviceId);

    adapter->setCaptureCompleteCallback(onCaptureComplete);

    if (!adapter->initialize()) {
        std::cerr << "Camera init failed: " << adapter->getDeviceInfo().lastError << std::endl;
        logging::Logger::getInstance().error("Camera init failed");
        return 1;
    }

    std::cout << "Camera ready. Commands: T = Take picture, S = Status, G = Pump EdsGetEvent (test), Q = Quit" << std::endl;

    while (true) {
        std::cout << "\n> ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        char choice = (line.size() == 1) ? static_cast<char>(std::toupper(static_cast<unsigned char>(line[0]))) : 0;

        if (choice == 'Q') {
            break;
        }

        if (choice == 'S') {
            auto info = adapter->getDeviceInfo();
            std::cout << "  State: " << devices::deviceStateToString(info.state)
                      << ", Name: " << info.deviceName
                      << ", LastError: " << info.lastError << std::endl;
            continue;
        }

        if (choice == 'G') {
            // G runs EdsGetEvent on main thread; normal pump is command-processor thread only. Use to see if any event is queued.
            const int n = 20;
            int okCount = adapter->pumpEvents(n);
            std::cout << "  EdsGetEvent() pumped " << n << " times, EDS_ERR_OK count=" << okCount
                      << " (check log for ObjectEvent: DirItemRequestTransfer/DirItemCreated if any)" << std::endl;
            continue;
        }

        if (choice == 'T') {
            g_captureDone = false;
            g_captureSuccess = false;
            g_captureIdReceived.clear();

            std::string captureId = "test_cap_" + std::to_string(
                static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));

            if (!adapter->capture(captureId)) {
                std::cerr << "  Capture failed: " << adapter->getDeviceInfo().lastError << std::endl;
                continue;
            }
            std::cout << "  Capture queued: " << captureId << " (waiting for download/complete...) " << std::endl;

            {
                std::unique_lock<std::mutex> lock(g_cvMutex);
                bool ok = g_cv.wait_for(lock, std::chrono::seconds(30), [] { return g_captureDone.load(); });
                if (!ok) {
                    std::cout << "  Timeout: no capture_complete in 30s. State may still be PROCESSING." << std::endl;
                    continue;
                }
            }

            if (g_captureSuccess) {
                std::cout << "  Capture complete: " << g_captureIdReceived << " (file saved)" << std::endl;
            } else {
                std::cout << "  Capture complete with error: " << g_captureIdReceived << std::endl;
            }
            continue;
        }

        std::cout << "  Unknown command. T=Take picture, S=Status, G=Pump EdsGetEvent, Q=Quit" << std::endl;
    }

    adapter->shutdown();
    logging::Logger::getInstance().info("test_edsdk_camera exiting");
    std::cout << "Bye." << std::endl;
    return 0;
}
