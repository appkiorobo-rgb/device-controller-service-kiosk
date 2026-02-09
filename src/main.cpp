// src/main.cpp
// Device Controller Service - Main Entry Point

// Include logger.h first to prevent Windows SDK conflicts
#include "logging/logger.h"
#include "core/service_core.h"
#include "config/config_manager.h"
#include "vendor_adapters/smartro/smartro_payment_adapter.h"
#include "vendor_adapters/canon/edsdk_camera_adapter.h"
#include "vendor_adapters/windows/windows_gdi_printer_adapter.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
    std::atomic<bool> g_running(true);
    core::ServiceCore* g_serviceCore = nullptr;
}

// Signal handler for graceful shutdown
void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    g_running = false;
    if (g_serviceCore) {
        g_serviceCore->stop();
    }
}

int main(int argc, char* argv[]) {
    // Initialize logger
    logging::Logger::getInstance().initialize("logs/service.log");
    logging::Logger::getInstance().info("Device Controller Service starting...");
    
    // Register signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    try {
        // config.ini: exe와 같은 폴더에 생성 (찾기 쉽게). 실패 시 CWD 사용.
        std::string configPath;
#ifdef _WIN32
        wchar_t exePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) != 0) {
            std::filesystem::path p(exePath);
            configPath = (p.parent_path() / "config.ini").string();
        }
#endif
        config::ConfigManager::getInstance().initialize(configPath);
        auto& config = config::ConfigManager::getInstance();

        // Create and start service core
        core::ServiceCore serviceCore;
        g_serviceCore = &serviceCore;

        // Register printer (Windows GDI only; no external app required). Config: printer.name
        std::string printerName = config.getPrinterName();
        std::string printerDeviceId = "windows_printer_001";
        logging::Logger::getInstance().info("Registering printer: " + printerDeviceId + " (" + printerName + ")");
        auto printerAdapter = std::make_shared<windows::WindowsGdiPrinterAdapter>(printerDeviceId, printerName);
        serviceCore.getDeviceManager().registerPrinter(printerDeviceId, printerAdapter);

        // Register payment terminal (Smartro) — config: payment.com_port, payment.enabled
        std::string comPort = config.getPaymentComPort();
        if (argc > 1) comPort = argv[1];
        std::string terminalId = "DEFAULT_TERM";
        if (argc > 2) terminalId = argv[2];
        std::string deviceId = "smartro_terminal_001";
        if (config.getPaymentEnabled()) {
            logging::Logger::getInstance().info("Registering payment terminal: " + deviceId + " on " + comPort);
            auto paymentAdapter = std::make_shared<smartro::SmartroPaymentAdapter>(
                deviceId, comPort, terminalId
            );
            serviceCore.getDeviceManager().registerPaymentTerminal(deviceId, paymentAdapter);
        } else {
            logging::Logger::getInstance().info("Payment terminal disabled in config");
        }
        
        // Register camera (EDSDK) — 초기화 실패해도 항상 등록. 자동감지 시 재연결 시도 가능.
        std::string cameraDeviceId = "canon_camera_001";
        logging::Logger::getInstance().info("Initializing EDSDK camera adapter: " + cameraDeviceId);
        
        auto cameraAdapter = std::make_shared<canon::EdsdkCameraAdapter>(cameraDeviceId);
        if (cameraAdapter->initialize()) {
            logging::Logger::getInstance().info("Camera registered successfully: " + cameraDeviceId);
        } else {
            logging::Logger::getInstance().warn("Camera not connected at startup. Use auto-detect after turning camera on.");
        }
        serviceCore.getDeviceManager().registerCamera(cameraDeviceId, cameraAdapter);

        // Setup event callbacks (capture_complete etc.) before starting IPC so they are set on the registered camera
        serviceCore.prepareEventCallbacks();

        // Start service
        if (!serviceCore.start()) {
            logging::Logger::getInstance().error("Failed to start service core");
            return 1;
        }
        
        logging::Logger::getInstance().info("Device Controller Service started successfully");
        std::cout << "Device Controller Service is running..." << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;
        
        // Main loop
        while (g_running && serviceCore.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Stop service
        serviceCore.stop();
        logging::Logger::getInstance().info("Device Controller Service stopped");
        
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Exception in main: " + std::string(e.what()));
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
