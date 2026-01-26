// check_status.cpp - Simple SMARTRO terminal status checker (COM4 only)
#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace device_controller::vendor::smartro;

int main() {
    std::cout << "=== SMARTRO Terminal Status Check (COM4 only) ===" << std::endl;
    
    // 1. Check if COM4 exists
    std::cout << "\n[1] Checking for COM4 port..." << std::endl;
    auto ports = SerialPort::enumeratePorts();
    
    bool com4Found = false;
    for (const auto& port : ports) {
        if (port == "COM4") {
            com4Found = true;
            break;
        }
    }
    
    if (!com4Found) {
        std::cerr << "ERROR: COM4 port not found." << std::endl;
        std::cout << "Available ports: ";
        for (size_t i = 0; i < ports.size(); i++) {
            std::cout << ports[i];
            if (i < ports.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl;
        return 1;
    }
    
    std::cout << "COM4 port found!" << std::endl;
    
    // 2. Test COM4
    std::cout << "\n[2] Testing port COM4..." << std::endl;
    const std::string port = "COM4";
    
    // Open port
    auto testPort = std::make_shared<SerialPort>();
    if (!testPort->open(port, 115200)) {
        std::cout << "  -> Failed to open port" << std::endl;
        return 1;
    }
    std::cout << "  -> Port opened successfully" << std::endl;
    
    // Initialize protocol
    auto testProtocol = std::make_shared<SMARTROProtocol>(testPort);
    if (!testProtocol->initialize()) {
        std::cout << "  -> Failed to initialize protocol" << std::endl;
        testPort->close();
        return 1;
    }
    std::cout << "  -> Protocol initialized successfully" << std::endl;
    
    // Send device check request
    std::cout << "[" << getTimestamp() << "]  -> Sending device check request..." << std::endl;
    if (!testProtocol->sendDeviceCheck()) {
        std::cout << "[" << getTimestamp() << "]  -> Device check failed (ACK timeout)" << std::endl;
        testProtocol->shutdown();
        testPort->close();
        return 1;
    }
    std::cout << "[" << getTimestamp() << "]  -> ACK received, waiting for response..." << std::endl;
    
    // Wait for response (max 2 seconds)
    auto startTime = std::chrono::steady_clock::now();
    bool responseTimeout = false;
    while (testProtocol->isWaitingForResponse()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if (elapsedMs > 2000) {
            std::cout << "[" << getTimestamp() << "]  -> Response timeout (" << elapsedMs << "ms)" << std::endl;
            responseTimeout = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // If timeout occurred, clean up and exit
    if (responseTimeout) {
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();
        std::cout << "[" << getTimestamp() << "]  -> Total wait time: " << elapsedMs << "ms" << std::endl;
        testProtocol->shutdown();
        testPort->close();
        return 1;
    }
    
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    std::cout << "[" << getTimestamp() << "]  -> Response received (" << elapsedMs << "ms)" << std::endl;
    
    // Check response
    if (!testProtocol->isWaitingForResponse()) {
        auto status = testProtocol->getLastDeviceStatus();
        
        // Print status regardless of module status
        std::cout << "\n=== Terminal Status ===" << std::endl;
        std::cout << "Port: " << port << std::endl;
        
        std::cout << "\nModule Status:" << std::endl;
        std::cout << "  Card Module: ";
        switch (status.cardModuleStatus) {
            case 'N': std::cout << "N (OK)"; break;
            case 'O': std::cout << "O (OK)"; break;
            case 'X': std::cout << "X (Error)"; break;
            default: std::cout << status.cardModuleStatus << " (Unknown)"; break;
        }
        std::cout << std::endl;
        
        std::cout << "  RF Module: ";
        switch (status.rfModuleStatus) {
            case 'O': std::cout << "O (OK)"; break;
            case 'X': std::cout << "X (Error)"; break;
            default: std::cout << status.rfModuleStatus << " (Unknown)"; break;
        }
        std::cout << std::endl;
        
        std::cout << "  VAN Server: ";
        switch (status.vanServerStatus) {
            case 'N': std::cout << "N (OK)"; break;
            case 'O': std::cout << "O (OK)"; break;
            case 'X': std::cout << "X (Error)"; break;
            case 'F': std::cout << "F (Connection Failed)"; break;
            default: std::cout << status.vanServerStatus << " (Unknown)"; break;
        }
        std::cout << std::endl;
        
        std::cout << "  Integration Server: ";
        switch (status.integrationServerStatus) {
            case 'N': std::cout << "N (OK)"; break;
            case 'O': std::cout << "O (OK)"; break;
            case 'X': std::cout << "X (Error)"; break;
            case 'F': std::cout << "F (Connection Failed)"; break;
            default: std::cout << status.integrationServerStatus << " (Unknown)"; break;
        }
        std::cout << std::endl;
        
        // Check if it's a SMARTRO terminal
        if (status.cardModuleStatus == 'O' || status.rfModuleStatus == 'O' || 
            status.vanServerStatus == 'O' || status.integrationServerStatus == 'O') {
            std::cout << "\n  -> SMARTRO terminal confirmed!" << std::endl;
        } else {
            std::cout << "\n  -> Not a SMARTRO terminal (all modules failed)" << std::endl;
        }
    } else {
        std::cout << "  -> No response received" << std::endl;
    }
    
    // Cleanup
    testProtocol->shutdown();
    testPort->close();
    
    std::cout << "\nDone!" << std::endl;
    return 0;
}
