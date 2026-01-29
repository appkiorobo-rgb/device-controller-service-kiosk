// include/vendor_adapters/canon/edsdk_camera_model.h
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

// Include EDSDK forward declarations (common header to avoid redefinition)
#include "vendor_adapters/canon/edsdk_forward_decls.h"

#include <string>
#include <functional>
#include <mutex>

namespace canon {

// Camera model for EDSDK operations
// Similar to Canon sample CameraModel but adapted for our architecture
class EdsdkCameraModel {
public:
    EdsdkCameraModel(EdsCameraRef camera);
    ~EdsdkCameraModel();
    
    // Get camera object
    EdsCameraRef getCameraObject() const { return camera_; }
    
    // Release camera ref on EDSDK thread only (called from CloseSessionCommand).
    // After this, getCameraObject() returns nullptr.
    void releaseCameraRef();
    
    // Property setters/getters
    void setPropertyUInt32(EdsPropertyID propertyID, EdsUInt32 value);
    void setPropertyString(EdsPropertyID propertyID, const EdsChar* str);
    
    // Event callbacks
    void setSessionOpenedCallback(std::function<void()> callback);
    void setSessionClosedCallback(std::function<void()> callback);
    void setDownloadCompleteCallback(std::function<void(const std::string&, const std::string&)> callback);
    void setErrorCallback(std::function<void(EdsError)> callback);
    void setObjectEventCallback(std::function<void(EdsUInt32, EdsBaseRef)> callback);
    
    // Notify methods (called by commands)
    void notifySessionOpened();
    void notifySessionClosed();
    void notifyDownloadComplete(const std::string& filePath, const std::string& captureId);
    void notifyError(EdsError error);
    void notifyObjectEvent(EdsUInt32 event, EdsBaseRef ref);
    
private:
    EdsCameraRef camera_;
    
    std::function<void()> sessionOpenedCallback_;
    std::function<void()> sessionClosedCallback_;
    std::function<void(const std::string&, const std::string&)> downloadCompleteCallback_;
    std::function<void(EdsError)> errorCallback_;
    std::function<void(EdsUInt32, EdsBaseRef)> objectEventCallback_;
    
    mutable std::mutex mutex_;
};

} // namespace canon
