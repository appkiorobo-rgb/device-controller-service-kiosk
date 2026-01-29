// include/vendor_adapters/canon/edsdk_camera_adapter.h
#pragma once

// Protect from Windows SDK conflicts BEFORE any includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"

// Include device types FIRST to ensure they are fully defined before EDSDK
#include "devices/device_types.h"
#include "devices/icamera.h"

// Include EDSDK forward declarations (common header to avoid redefinition)
#include "vendor_adapters/canon/edsdk_forward_decls.h"

// Include command processor (doesn't include EDSDK.h directly)
#include "vendor_adapters/canon/edsdk_command_processor.h"

// Include config manager (doesn't include EDSDK.h)
#include "config/config_manager.h"

// Include camera model AFTER device types and EDSDK forward declarations
// edsdk_camera_model.h uses forward declarations (no edsdk_wrapper.h in header)
#include "vendor_adapters/canon/edsdk_camera_model.h"

#include <string>
#include <memory>
#include <mutex>
#include <future>
#include <map>
#include <atomic>
#include <vector>
#include <cstdint>
#include <chrono>

namespace canon {

// EDSDK Camera Adapter - implements ICamera interface
class EdsdkCameraAdapter : public devices::ICamera {
public:
    EdsdkCameraAdapter(const std::string& deviceId);
    ~EdsdkCameraAdapter();
    
    // ICamera interface implementation
    devices::DeviceInfo getDeviceInfo() const override;
    bool capture(const std::string& captureId) override;
    devices::DeviceState getState() const override;
    bool startPreview() override;
    bool stopPreview() override;
    bool setSettings(const devices::CameraSettings& settings) override;
    devices::CameraSettings getSettings() const override;
    void setCaptureCompleteCallback(std::function<void(const devices::CaptureCompleteEvent&)> callback) override;
    void setStateChangedCallback(std::function<void(devices::DeviceState)> callback) override;
    
    // Initialize EDSDK and discover cameras
    bool initialize();
    
    // Shutdown EDSDK
    void shutdown();
    
    // Manual EdsGetEvent() pump (for testing: G key). Returns number of times EdsGetEvent() returned EDS_ERR_OK.
    int pumpEvents(int maxCalls);
    
    // Called from InitializeCameraCommand (EDSDK thread) when init finishes
    void onInitComplete(bool success);
    // Called from InitializeCameraCommand (EDSDK thread) to set model and device name
    void setCameraModelAndDeviceName(std::unique_ptr<EdsdkCameraModel> model, const std::string& deviceName);
    // For commands that need the model (e.g. TakePictureCommand). Non-owning.
    EdsdkCameraModel* getCameraModel() { return cameraModel_.get(); }
    // Increment SDK ref count (called from InitializeCameraCommand on EDSDK thread)
    void incrementSdkRefCount();
    // Decrement and optionally EdsTerminateSDK (called from command on EDSDK thread on init failure)
    void decrementSdkRefCountAndMaybeTerminate();

private:
    void disconnectCamera();
    
    // Event handlers
    void onSessionOpened();
    void onSessionClosed();
    void onDownloadComplete(const std::string& filePath, const std::string& captureId);
    void onError(EdsError error);
    void onObjectEvent(EdsUInt32 event, EdsBaseRef ref);
    
    // Helper methods
    void updateState(devices::DeviceState newState);
    void setLastError(const std::string& error);
    std::vector<uint8_t> readImageFile(const std::string& filePath) const;
    
    std::string deviceId_;
    std::string deviceName_;
    devices::DeviceState state_;
    std::string lastError_;
    std::chrono::system_clock::time_point lastUpdateTime_;
    
    // EDSDK objects (forward declared types)
    EdsCameraRef cameraRef_;
    std::unique_ptr<EdsdkCameraModel> cameraModel_;
    std::unique_ptr<EdsdkCommandProcessor> commandProcessor_;
    
    // Capture tracking
    std::map<std::string, std::string> pendingCaptures_; // captureId -> expected filename
    std::mutex captureMutex_;
    
    // Callbacks
    std::function<void(const devices::CaptureCompleteEvent&)> captureCompleteCallback_;
    std::function<void(devices::DeviceState)> stateChangedCallback_;
    
    mutable std::mutex stateMutex_;
    
    // Settings
    devices::CameraSettings settings_;
    
    // EDSDK initialization flag
    std::atomic<bool> sdkInitialized_;
    static std::atomic<int> sdkRefCount_;
    static std::mutex sdkMutex_;

    std::promise<bool> initPromise_;
};

} // namespace canon
