// src/core/device_manager.cpp
#include "core/device_manager.h"

namespace core {

DeviceManager::DeviceManager() {
}

void DeviceManager::registerPaymentTerminal(const std::string& deviceId,
                                            std::shared_ptr<devices::IPaymentTerminal> terminal) {
    std::lock_guard<std::mutex> lock(mutex_);
    paymentTerminals_[deviceId] = terminal;
}

void DeviceManager::registerPrinter(const std::string& deviceId,
                                    std::shared_ptr<devices::IPrinter> printer) {
    std::lock_guard<std::mutex> lock(mutex_);
    printers_[deviceId] = printer;
}

void DeviceManager::registerCamera(const std::string& deviceId,
                                  std::shared_ptr<devices::ICamera> camera) {
    std::lock_guard<std::mutex> lock(mutex_);
    cameras_[deviceId] = camera;
}

std::shared_ptr<devices::IPaymentTerminal> DeviceManager::getPaymentTerminal(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = paymentTerminals_.find(deviceId);
    if (it != paymentTerminals_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<devices::IPrinter> DeviceManager::getPrinter(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = printers_.find(deviceId);
    if (it != printers_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<devices::ICamera> DeviceManager::getCamera(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = cameras_.find(deviceId);
    if (it != cameras_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<devices::IPaymentTerminal> DeviceManager::getDefaultPaymentTerminal() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!paymentTerminals_.empty()) {
        return paymentTerminals_.begin()->second;
    }
    return nullptr;
}

std::shared_ptr<devices::IPrinter> DeviceManager::getDefaultPrinter() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!printers_.empty()) {
        return printers_.begin()->second;
    }
    return nullptr;
}

std::shared_ptr<devices::ICamera> DeviceManager::getDefaultCamera() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cameras_.empty()) {
        return cameras_.begin()->second;
    }
    return nullptr;
}

std::vector<devices::DeviceInfo> DeviceManager::getAllDeviceInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<devices::DeviceInfo> result;
    
    for (const auto& pair : paymentTerminals_) {
        result.push_back(pair.second->getDeviceInfo());
    }
    
    for (const auto& pair : printers_) {
        result.push_back(pair.second->getDeviceInfo());
    }
    
    for (const auto& pair : cameras_) {
        result.push_back(pair.second->getDeviceInfo());
    }
    
    return result;
}

std::vector<std::string> DeviceManager::getDeviceIds(devices::DeviceType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    
    switch (type) {
        case devices::DeviceType::PAYMENT_TERMINAL:
            for (const auto& pair : paymentTerminals_) {
                result.push_back(pair.first);
            }
            break;
        case devices::DeviceType::PRINTER:
            for (const auto& pair : printers_) {
                result.push_back(pair.first);
            }
            break;
        case devices::DeviceType::CAMERA:
            for (const auto& pair : cameras_) {
                result.push_back(pair.first);
            }
            break;
    }
    
    return result;
}

} // namespace core
