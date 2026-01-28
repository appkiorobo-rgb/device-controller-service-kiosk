// include/devices/device_types.h
#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <functional>

namespace devices {

// Device type
enum class DeviceType {
    PAYMENT_TERMINAL,
    PRINTER,
    CAMERA
};

// Device state (common)
enum class DeviceState {
    DISCONNECTED = 0,  // Not connected
    CONNECTING = 1,    // Connecting
    READY = 2,         // Ready
    PROCESSING = 3,    // Processing
    ERROR = 4,         // Error occurred
    HUNG = 5           // No response (timeout)
};

// Convert device type to string
inline std::string deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::PAYMENT_TERMINAL: return "payment";
        case DeviceType::PRINTER: return "printer";
        case DeviceType::CAMERA: return "camera";
        default: return "unknown";
    }
}

// Convert string to device type
inline DeviceType stringToDeviceType(const std::string& str) {
    if (str == "payment") return DeviceType::PAYMENT_TERMINAL;
    if (str == "printer") return DeviceType::PRINTER;
    if (str == "camera") return DeviceType::CAMERA;
    return DeviceType::PAYMENT_TERMINAL; // Default
}

// Convert device state to string
inline std::string deviceStateToString(DeviceState state) {
    switch (state) {
        case DeviceState::DISCONNECTED: return "DISCONNECTED";
        case DeviceState::CONNECTING: return "CONNECTING";
        case DeviceState::READY: return "READY";
        case DeviceState::PROCESSING: return "PROCESSING";
        case DeviceState::ERROR: return "ERROR";
        case DeviceState::HUNG: return "HUNG";
        default: return "UNKNOWN";
    }
}

// Device basic information
struct DeviceInfo {
    std::string deviceId;
    DeviceType deviceType;
    std::string deviceName;
    DeviceState state;
    std::string lastError;
    std::chrono::system_clock::time_point lastUpdateTime;
};

// Event callback type
template<typename EventData>
using EventCallback = std::function<void(const EventData&)>;

} // namespace devices
