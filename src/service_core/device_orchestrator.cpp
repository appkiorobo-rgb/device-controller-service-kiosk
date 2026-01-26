// src/service_core/device_orchestrator.cpp
#include "service_core/device_orchestrator.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>

namespace device_controller {

DeviceOrchestrator::DeviceOrchestrator() {
}

DeviceOrchestrator::~DeviceOrchestrator() {
    shutdownAll();
}

void DeviceOrchestrator::registerCamera(std::shared_ptr<ICamera> camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    cameras_.push_back(camera);
}

void DeviceOrchestrator::registerPrinter(std::shared_ptr<IPrinter> printer) {
    std::lock_guard<std::mutex> lock(mutex_);
    printers_.push_back(printer);
}

void DeviceOrchestrator::registerPaymentTerminal(std::shared_ptr<IPaymentTerminal> terminal) {
    std::lock_guard<std::mutex> lock(mutex_);
    paymentTerminals_.push_back(terminal);
}

std::shared_ptr<ICamera> DeviceOrchestrator::getCamera(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deviceId.empty() && !cameras_.empty()) {
        return cameras_[0];
    }
    auto it = std::find_if(cameras_.begin(), cameras_.end(),
        [&deviceId](const auto& cam) { return cam->getDeviceId() == deviceId; });
    return (it != cameras_.end()) ? *it : nullptr;
}

std::shared_ptr<IPrinter> DeviceOrchestrator::getPrinter(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deviceId.empty() && !printers_.empty()) {
        return printers_[0];
    }
    auto it = std::find_if(printers_.begin(), printers_.end(),
        [&deviceId](const auto& printer) { return printer->getDeviceId() == deviceId; });
    return (it != printers_.end()) ? *it : nullptr;
}

std::shared_ptr<IPaymentTerminal> DeviceOrchestrator::getPaymentTerminal(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deviceId.empty() && !paymentTerminals_.empty()) {
        return paymentTerminals_[0];
    }
    auto it = std::find_if(paymentTerminals_.begin(), paymentTerminals_.end(),
        [&deviceId](const auto& terminal) { return terminal->getDeviceId() == deviceId; });
    return (it != paymentTerminals_.end()) ? *it : nullptr;
}

void DeviceOrchestrator::initializeAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "[ORCHESTRATOR] Initializing all devices..." << std::endl;
    std::cout << "[ORCHESTRATOR] Registered devices - Cameras: " << cameras_.size() 
              << ", Printers: " << printers_.size() 
              << ", Payment Terminals: " << paymentTerminals_.size() << std::endl;
    
    // Initialize cameras
    for (auto& camera : cameras_) {
        camera->initialize();
    }
    
    // Initialize printers
    for (auto& printer : printers_) {
        printer->initialize();
    }
    
    // Initialize payment terminals (this will trigger actual device detection)
    for (auto& terminal : paymentTerminals_) {
        terminal->initialize();
    }
    
    std::cout << "[ORCHESTRATOR] All devices initialization complete" << std::endl;
}

void DeviceOrchestrator::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& camera : cameras_) {
        camera->shutdown();
    }
    for (auto& printer : printers_) {
        printer->shutdown();
    }
    for (auto& terminal : paymentTerminals_) {
        terminal->shutdown();
    }
}

nlohmann::json DeviceOrchestrator::getStateSnapshot(const std::vector<std::string>& deviceTypes) const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json snapshot;

    bool includeAll = deviceTypes.empty();
    bool includeCamera = includeAll || std::find(deviceTypes.begin(), deviceTypes.end(), "camera") != deviceTypes.end();
    bool includePrinter = includeAll || std::find(deviceTypes.begin(), deviceTypes.end(), "printer") != deviceTypes.end();
    bool includePayment = includeAll || std::find(deviceTypes.begin(), deviceTypes.end(), "payment") != deviceTypes.end();

    if (includeCamera) {
        nlohmann::json cameras = nlohmann::json::array();
        for (const auto& camera : cameras_) {
            cameras.push_back({
                {"deviceId", camera->getDeviceId()},
                {"deviceName", camera->getDeviceName()},
                {"state", static_cast<int>(camera->getState())}
            });
        }
        snapshot["cameras"] = cameras;
    }

    if (includePrinter) {
        nlohmann::json printers = nlohmann::json::array();
        for (const auto& printer : printers_) {
            printers.push_back({
                {"deviceId", printer->getDeviceId()},
                {"deviceName", printer->getDeviceName()},
                {"state", static_cast<int>(printer->getState())}
            });
        }
        snapshot["printers"] = printers;
    }

    if (includePayment) {
        nlohmann::json terminals = nlohmann::json::array();
        for (const auto& terminal : paymentTerminals_) {
            terminals.push_back({
                {"deviceId", terminal->getDeviceId()},
                {"deviceName", terminal->getDeviceName()},
                {"state", static_cast<int>(terminal->getState())}
            });
        }
        snapshot["terminals"] = terminals;
    }

    return snapshot;
}

} // namespace device_controller
