// include/devices/device_types.h
#pragma once

// Protect from Windows SDK conflicts
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

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
// Use STATE_ prefix to avoid Windows/EDSDK macros (ERROR, CONNECTING, READY, PROCESSING)
enum class DeviceState {
    DISCONNECTED = 0,   // Not connected
    STATE_CONNECTING = 1,
    STATE_READY = 2,
    STATE_PROCESSING = 3,
    STATE_ERROR = 4,
    HUNG = 5            // No response (timeout)
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
        case DeviceState::STATE_CONNECTING: return "CONNECTING";
        case DeviceState::STATE_READY: return "READY";
        case DeviceState::STATE_PROCESSING: return "PROCESSING";
        case DeviceState::STATE_ERROR: return "ERROR";
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
