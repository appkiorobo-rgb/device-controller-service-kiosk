// include/device_abstraction/icamera.h
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <vector>

namespace device_controller {

// Camera state machine
enum class CameraState {
    DISCONNECTED,
    CONNECTING,
    READY,
    CAPTURING,
    ERROR,
    HUNG
};

// Camera event types
enum class CameraEventType {
    STATE_CHANGED,
    CAPTURE_COMPLETE,
    CAPTURE_FAILED,
    ERROR_OCCURRED
};

// Camera event data
struct CameraEvent {
    CameraEventType type;
    CameraState state;
    std::string errorCode;
    std::string errorMessage;
    std::string imagePath;  // Path to captured image file
    std::chrono::milliseconds timestamp;
};

// Event callback type
using CameraEventCallback = std::function<void(const CameraEvent&)>;

// ICamera interface - stable abstraction for camera devices
class ICamera {
public:
    virtual ~ICamera() = default;

    // Get current state
    virtual CameraState getState() const = 0;

    // Initialize camera connection
    // Returns true if initialization started, false if already initialized or error
    virtual bool initialize() = 0;

    // Shutdown camera connection
    virtual void shutdown() = 0;

    // Start capture operation
    // Does not return image directly - result comes via event callback
    // Returns true if capture started, false if rejected
    virtual bool startCapture() = 0;

    // Cancel ongoing capture
    virtual void cancelCapture() = 0;

    // Register event callback
    virtual void setEventCallback(CameraEventCallback callback) = 0;

    // Get device information
    virtual std::string getDeviceId() const = 0;
    virtual std::string getDeviceName() const = 0;
};

} // namespace device_controller
