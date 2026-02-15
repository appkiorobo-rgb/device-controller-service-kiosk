// src/devices/payment_terminal_factory.cpp
#include "devices/payment_terminal_factory.h"
#include "logging/logger.h"

namespace devices {

std::mutex& PaymentTerminalFactory::mutex() {
    static std::mutex m;
    return m;
}

std::vector<PaymentTerminalFactory::VendorProbe>& PaymentTerminalFactory::vendors() {
    static std::vector<VendorProbe> v;
    return v;
}

void PaymentTerminalFactory::registerVendor(VendorProbe probe) {
    std::lock_guard<std::mutex> lock(mutex());
    vendors().push_back(std::move(probe));
    logging::Logger::getInstance().info("PaymentTerminalFactory: registered vendor \"" + vendors().back().vendorName + "\"");
}

std::vector<std::string> PaymentTerminalFactory::getRegisteredVendors() {
    std::lock_guard<std::mutex> lock(mutex());
    std::vector<std::string> names;
    names.reserve(vendors().size());
    for (const auto& v : vendors()) names.push_back(v.vendorName);
    return names;
}

std::pair<std::string, std::shared_ptr<IPaymentTerminal>>
PaymentTerminalFactory::createForPort(const std::string& deviceId, const std::string& port,
                                       const std::string& category) {
    std::lock_guard<std::mutex> lock(mutex());
    for (const auto& v : vendors()) {
        // Skip vendors that don't match the requested category
        if (!category.empty() && v.category != category) continue;
        try {
            logging::Logger::getInstance().debug(
                "PaymentTerminalFactory: trying vendor \"" + v.vendorName + "\" (category=" + v.category + ") on " + port);
            if (v.tryPort(port)) {
                auto adapter = v.create(deviceId, port);
                if (adapter) {
                    logging::Logger::getInstance().info(
                        "PaymentTerminalFactory: vendor \"" + v.vendorName + "\" detected on " + port);
                    return {v.vendorName, adapter};
                }
            }
        } catch (const std::exception& e) {
            logging::Logger::getInstance().debug(
                "PaymentTerminalFactory: vendor \"" + v.vendorName + "\" probe failed on " + port + ": " + e.what());
        }
    }
    return {"", nullptr};
}

std::pair<std::string, std::shared_ptr<IPaymentTerminal>>
PaymentTerminalFactory::detectOnPorts(const std::string& deviceId,
                                       const std::vector<std::string>& ports,
                                       const std::string& excludePort,
                                       const std::string& category) {
    for (const auto& port : ports) {
        if (!excludePort.empty() && port == excludePort) continue;
        auto result = createForPort(deviceId, port, category);
        if (result.second) return result;
    }
    return {"", nullptr};
}

void PaymentTerminalFactory::clearVendors() {
    std::lock_guard<std::mutex> lock(mutex());
    vendors().clear();
}

} // namespace devices
