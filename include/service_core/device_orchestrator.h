// include/service_core/device_orchestrator.h
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>
#include "device_abstraction/icamera.h"
#include "device_abstraction/iprinter.h"
#include "device_abstraction/ipayment_terminal.h"

namespace device_controller {

// DeviceOrchestrator - manages all devices and their state machines
// Part of Service Core layer
class DeviceOrchestrator {
public:
    DeviceOrchestrator();
    ~DeviceOrchestrator();

    // Device registration (called during initialization)
    void registerCamera(std::shared_ptr<ICamera> camera);
    void registerPrinter(std::shared_ptr<IPrinter> printer);
    void registerPaymentTerminal(std::shared_ptr<IPaymentTerminal> terminal);

    // Device access
    std::shared_ptr<ICamera> getCamera(const std::string& deviceId = "");
    std::shared_ptr<IPrinter> getPrinter(const std::string& deviceId = "");
    std::shared_ptr<IPaymentTerminal> getPaymentTerminal(const std::string& deviceId = "");

    // Initialize all devices
    void initializeAll();

    // Shutdown all devices
    void shutdownAll();

    // Get state snapshot for IPC
    nlohmann::json getStateSnapshot(const std::vector<std::string>& deviceTypes = {}) const;

private:
    std::mutex mutex_;
    std::vector<std::shared_ptr<ICamera>> cameras_;
    std::vector<std::shared_ptr<IPrinter>> printers_;
    std::vector<std::shared_ptr<IPaymentTerminal>> paymentTerminals_;
};

} // namespace device_controller
