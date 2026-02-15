// include/core/device_constants.h
// Shared device ID constants and utility functions used across service_core modules.
#pragma once

#include <string>
#include <map>

namespace core {

// --- Device ID constants ---
constexpr const char* kCardTerminalId = "card_terminal_001";
constexpr const char* kCashDeviceId   = "lv77_cash_001";

// --- Config helper ---
/// Check if a key is enabled: payload takes priority over config.
/// Returns true when the value is "1", "true", or "yes".
inline bool isEnabled(const std::map<std::string, std::string>& payload,
                      const std::map<std::string, std::string>& cfg,
                      const char* key) {
    auto it = payload.find(key);
    if (it != payload.end() && !it->second.empty())
        return (it->second == "1" || it->second == "true" || it->second == "yes");
    auto c = cfg.find(key);
    if (c != cfg.end())
        return (c->second == "1" || c->second == "true" || c->second == "yes");
    return false;
}

} // namespace core
