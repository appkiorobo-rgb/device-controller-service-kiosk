// include/core/device_manager.h
#pragma once

#include "devices/ipayment_terminal.h"
#include "devices/iprinter.h"
#include "devices/icamera.h"
#include "devices/device_types.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <mutex>

namespace core {

// Device Manager
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager() = default;
    
    // Register payment terminal
    void registerPaymentTerminal(const std::string& deviceId, 
                                 std::shared_ptr<devices::IPaymentTerminal> terminal);
    
    // Register printer (future)
    void registerPrinter(const std::string& deviceId,
                        std::shared_ptr<devices::IPrinter> printer);
    
    // Register camera (future)
    void registerCamera(const std::string& deviceId,
                       std::shared_ptr<devices::ICamera> camera);
    
    // Get device
    std::shared_ptr<devices::IPaymentTerminal> getPaymentTerminal(const std::string& deviceId);
    std::shared_ptr<devices::IPrinter> getPrinter(const std::string& deviceId);
    std::shared_ptr<devices::ICamera> getCamera(const std::string& deviceId);
    
    // Get default device (first registered device)
    std::shared_ptr<devices::IPaymentTerminal> getDefaultPaymentTerminal();
    std::shared_ptr<devices::IPrinter> getDefaultPrinter();
    std::shared_ptr<devices::ICamera> getDefaultCamera();
    
    // Get all device information
    std::vector<devices::DeviceInfo> getAllDeviceInfo() const;
    
    // Get device list by type
    std::vector<std::string> getDeviceIds(devices::DeviceType type) const;
    
private:
    std::map<std::string, std::shared_ptr<devices::IPaymentTerminal>> paymentTerminals_;
    std::map<std::string, std::shared_ptr<devices::IPrinter>> printers_;
    std::map<std::string, std::shared_ptr<devices::ICamera>> cameras_;
    mutable std::mutex mutex_;
};

} // namespace core
