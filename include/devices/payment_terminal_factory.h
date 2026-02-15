// include/devices/payment_terminal_factory.h
// Factory for auto-detecting and creating payment terminal adapters.
// Register vendor probes at startup; during detect_hardware, iterate COM ports
// and pick the first vendor whose protocol responds.
#pragma once

#include "devices/ipayment_terminal.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

namespace devices {

class PaymentTerminalFactory {
public:
    /// Describes one vendor's probe + create pair.
    struct VendorProbe {
        std::string vendorName;
        /// Device category: "card" for card payment terminals, "cash" for cash devices.
        std::string category;
        /// Returns true if a terminal of this vendor responds on `port`.
        std::function<bool(const std::string& port)> tryPort;
        /// Create an adapter instance for the given deviceId / port.
        std::function<std::shared_ptr<IPaymentTerminal>(
            const std::string& deviceId, const std::string& port)> create;
    };

    /// Register a vendor probe (call once at startup per vendor).
    static void registerVendor(VendorProbe probe);

    /// Ordered list of registered vendor names.
    static std::vector<std::string> getRegisteredVendors();

    /// Try all registered vendors on `port`; return the first that responds.
    /// If `category` is non-empty, only try vendors matching that category.
    /// Returns (vendorName, adapter) on success, ("", nullptr) on failure.
    static std::pair<std::string, std::shared_ptr<IPaymentTerminal>>
        createForPort(const std::string& deviceId, const std::string& port,
                      const std::string& category = "");

    /// Scan `ports` (excluding `excludePort`) with all registered vendors.
    /// If `category` is non-empty, only try vendors matching that category.
    /// Returns (vendorName, adapter) for the first port+vendor that responds.
    static std::pair<std::string, std::shared_ptr<IPaymentTerminal>>
        detectOnPorts(const std::string& deviceId,
                      const std::vector<std::string>& ports,
                      const std::string& excludePort = "",
                      const std::string& category = "");

    /// Reset all registered vendors (for tests).
    static void clearVendors();

private:
    static std::mutex& mutex();
    static std::vector<VendorProbe>& vendors();
};

} // namespace devices
