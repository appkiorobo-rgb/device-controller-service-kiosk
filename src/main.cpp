// src/main.cpp
// Device Controller Service - Main Entry Point

// Include logger.h first to prevent Windows SDK conflicts
#include "logging/logger.h"
#include "core/service_core.h"
#include "vendor_adapters/smartro/smartro_payment_adapter.h"
#include "vendor_adapters/canon/edsdk_camera_adapter.h"
#include "config/config_manager.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

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
        // Initialize configuration
        config::ConfigManager::getInstance().initialize();
        
        // Create and start service core
        core::ServiceCore serviceCore;
        g_serviceCore = &serviceCore;
        
        // Register payment terminal (Smartro)
        // TODO: COM port and terminal ID should come from config file or command line
        std::string comPort = "COM3";  // Default, should be configurable
        std::string terminalId = "DEFAULT_TERM";
        std::string deviceId = "smartro_terminal_001";
        
        // Check if COM port is provided as argument
        if (argc > 1) {
            comPort = argv[1];
        }
        if (argc > 2) {
            terminalId = argv[2];
        }
        
        logging::Logger::getInstance().info("Registering payment terminal: " + deviceId + " on " + comPort);
        
        auto paymentAdapter = std::make_shared<smartro::SmartroPaymentAdapter>(
            deviceId, comPort, terminalId
        );
        
        serviceCore.getDeviceManager().registerPaymentTerminal(deviceId, paymentAdapter);
        
        // Register camera (EDSDK)
        std::string cameraDeviceId = "canon_camera_001";
        logging::Logger::getInstance().info("Initializing EDSDK camera adapter: " + cameraDeviceId);
        
        auto cameraAdapter = std::make_shared<canon::EdsdkCameraAdapter>(cameraDeviceId);
        
        if (cameraAdapter->initialize()) {
            serviceCore.getDeviceManager().registerCamera(cameraDeviceId, cameraAdapter);
            logging::Logger::getInstance().info("Camera registered successfully: " + cameraDeviceId);
        } else {
            logging::Logger::getInstance().warn("Failed to initialize camera adapter. Camera features will not be available.");
        }

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
