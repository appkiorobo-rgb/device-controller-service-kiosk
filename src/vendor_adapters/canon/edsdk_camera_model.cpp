// src/vendor_adapters/canon/edsdk_camera_model.cpp
// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"

// Include device types BEFORE EDSDK to avoid conflicts
#include "devices/device_types.h"

// Include EDSDK wrapper (includes EDSDK.h safely) AFTER device types
#include "vendor_adapters/canon/edsdk_wrapper.h"

#include "vendor_adapters/canon/edsdk_camera_model.h"

namespace canon {

EdsdkCameraModel::EdsdkCameraModel(EdsCameraRef camera)
    : camera_(camera) {
    if (camera_) {
        EdsRetain(camera_);
    }
}

EdsdkCameraModel::~EdsdkCameraModel() {
    if (camera_) {
        EdsRelease(camera_);
        camera_ = nullptr;
    }
}

void EdsdkCameraModel::releaseCameraRef() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (camera_) {
        EdsRelease(camera_);
        camera_ = nullptr;
    }
}

void EdsdkCameraModel::setPropertyUInt32(EdsPropertyID propertyID, EdsUInt32 value) {
    // Store property if needed (for now, just log)
    logging::Logger::getInstance().debug("Property set: " + std::to_string(propertyID) + " = " + std::to_string(value));
}

void EdsdkCameraModel::setPropertyString(EdsPropertyID propertyID, const EdsChar* str) {
    // Store property if needed (for now, just log)
    logging::Logger::getInstance().debug("Property set: " + std::to_string(propertyID) + " = " + (str ? str : "null"));
}

void EdsdkCameraModel::setSessionOpenedCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionOpenedCallback_ = callback;
}

void EdsdkCameraModel::setSessionClosedCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessionClosedCallback_ = callback;
}

void EdsdkCameraModel::setDownloadCompleteCallback(std::function<void(const std::string&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    downloadCompleteCallback_ = callback;
}

void EdsdkCameraModel::setErrorCallback(std::function<void(EdsError)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorCallback_ = callback;
}

void EdsdkCameraModel::setObjectEventCallback(std::function<void(EdsUInt32, EdsBaseRef)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    objectEventCallback_ = callback;
}

void EdsdkCameraModel::notifySessionOpened() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessionOpenedCallback_) {
        sessionOpenedCallback_();
    }
}

void EdsdkCameraModel::notifySessionClosed() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessionClosedCallback_) {
        sessionClosedCallback_();
    }
}

void EdsdkCameraModel::notifyDownloadComplete(const std::string& filePath, const std::string& captureId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (downloadCompleteCallback_) {
        downloadCompleteCallback_(filePath, captureId);
    }
}

void EdsdkCameraModel::notifyError(EdsError error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (errorCallback_) {
        errorCallback_(error);
    }
}

void EdsdkCameraModel::notifyObjectEvent(EdsUInt32 event, EdsBaseRef ref) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (objectEventCallback_) {
        objectEventCallback_(event, ref);
    }
}

} // namespace canon
