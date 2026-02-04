// src/vendor_adapters/canon/edsdk_camera_adapter.cpp
// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"

// Include device types BEFORE EDSDK to avoid conflicts
#include "devices/device_types.h"
#include "devices/icamera.h"

// Include EDSDK wrapper (includes EDSDK.h safely) AFTER device types
#include "vendor_adapters/canon/edsdk_wrapper.h"

#include "vendor_adapters/canon/edsdk_camera_adapter.h"
#include "vendor_adapters/canon/edsdk_commands.h"
#include "vendor_adapters/canon/edsdk_event_handler.h"
#include "config/config_manager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace canon {

// Static members for EDSDK initialization tracking
std::atomic<int> EdsdkCameraAdapter::sdkRefCount_(0);
std::mutex EdsdkCameraAdapter::sdkMutex_;

EdsdkCameraAdapter::EdsdkCameraAdapter(const std::string& deviceId)
    : deviceId_(deviceId)
    , state_(devices::DeviceState::DISCONNECTED)
    , cameraRef_(nullptr)
    , sdkInitialized_(false) {
    
    lastUpdateTime_ = std::chrono::system_clock::now();
    
    // Default settings
    settings_.resolutionWidth = 0;  // Use camera default
    settings_.resolutionHeight = 0;
    settings_.imageFormat = "jpeg";
    settings_.quality = 95;
    settings_.autoFocus = true;
}

EdsdkCameraAdapter::~EdsdkCameraAdapter() {
    shutdown();
}

bool EdsdkCameraAdapter::initialize() {
    updateState(devices::DeviceState::STATE_CONNECTING);
    sdkInitialized_ = true;

    commandProcessor_ = std::make_unique<EdsdkCommandProcessor>();
    if (!commandProcessor_->start()) {
        setLastError("Failed to start command processor");
        updateState(devices::DeviceState::STATE_ERROR);
        return false;
    }

    EdsdkInitCallbacks cbs;
    cbs.onSessionOpened = [this]() { onSessionOpened(); };
    cbs.onSessionClosed = [this]() { onSessionClosed(); };
    cbs.onDownloadComplete = [this](const std::string& path, const std::string& id) { onDownloadComplete(path, id); };
    cbs.onError = [this](EdsError err) { onError(err); };
    cbs.onObjectEvent = [this](EdsUInt32 event, EdsBaseRef ref) { onObjectEvent(event, ref); };

    initPromise_ = std::promise<bool>();
    std::future<bool> initFuture = initPromise_.get_future();
    auto initCmd = std::make_shared<InitializeCameraCommand>(this, cbs);
    commandProcessor_->enqueue(initCmd);

    bool ok = false;
    try {
        ok = initFuture.get();
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("InitializeCameraCommand exception: " + std::string(e.what()));
    }
    if (!ok) {
        updateState(devices::DeviceState::DISCONNECTED);
        return false;
    }
    logging::Logger::getInstance().info("EDSDK Camera Adapter init command completed (READY set by onSessionOpened): " + deviceId_);
    return true;
}

void EdsdkCameraAdapter::onInitComplete(bool success) {
    try {
        initPromise_.set_value(success);
    } catch (...) {
        // set_value already called
    }
}

void EdsdkCameraAdapter::setCameraModelAndDeviceName(std::unique_ptr<EdsdkCameraModel> model, const std::string& deviceName) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    cameraRef_ = nullptr;
    cameraModel_ = std::move(model);
    if (cameraModel_) {
        cameraRef_ = cameraModel_->getCameraObject();
    }
    deviceName_ = deviceName;
}

void EdsdkCameraAdapter::incrementSdkRefCount() {
    std::lock_guard<std::mutex> lock(sdkMutex_);
    sdkRefCount_++;
}

void EdsdkCameraAdapter::decrementSdkRefCountAndMaybeTerminate() {
    std::lock_guard<std::mutex> lock(sdkMutex_);
    if (sdkRefCount_ > 0) {
        sdkRefCount_--;
        if (sdkRefCount_ == 0) {
            EdsTerminateSDK();
            logging::Logger::getInstance().info("EDSDK terminated (init failure on command processor thread)");
        }
    }
}

void EdsdkCameraAdapter::shutdown() {
    if (!sdkInitialized_) {
        return;
    }
    stopPreview();

    if (commandProcessor_) {
        if (cameraModel_) {
            auto onClosed = [this]() {
                std::lock_guard<std::mutex> lock(sdkMutex_);
                if (sdkRefCount_ > 0) {
                    sdkRefCount_--;
                    if (sdkRefCount_ == 0) {
                        EdsTerminateSDK();
                        logging::Logger::getInstance().info("EDSDK terminated (on command processor thread)");
                    }
                }
            };
            auto closeCmd = std::make_shared<CloseSessionCommand>(cameraModel_.get(), std::move(onClosed));
            commandProcessor_->setCloseCommand(closeCmd);
        }
        commandProcessor_->stop();
        commandProcessor_->join();
        commandProcessor_.reset();
    }

    disconnectCamera();
    sdkInitialized_ = false;
    updateState(devices::DeviceState::DISCONNECTED);
}

void EdsdkCameraAdapter::disconnectCamera() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    cameraRef_ = nullptr;
    cameraModel_.reset();
}

devices::DeviceInfo EdsdkCameraAdapter::getDeviceInfo() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    devices::DeviceInfo info;
    info.deviceId = deviceId_;
    info.deviceType = devices::DeviceType::CAMERA;
    info.deviceName = deviceName_;
    info.state = state_;
    info.lastError = lastError_;
    info.lastUpdateTime = lastUpdateTime_;
    
    return info;
}

bool EdsdkCameraAdapter::capture(const std::string& captureId) {
    std::function<void(devices::DeviceState)> stateCb;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        // If stuck in PROCESSING (e.g. download event never came), recover after 30s so next capture can run
        if (state_ == devices::DeviceState::STATE_PROCESSING) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - lastUpdateTime_).count();
            if (elapsed >= 30) {
                state_ = devices::DeviceState::STATE_READY;
                lastUpdateTime_ = std::chrono::system_clock::now();
                {
                    std::lock_guard<std::mutex> captureLock(captureMutex_);
                    pendingCaptures_.clear();
                }
                logging::Logger::getInstance().warn("Camera PROCESSING timeout (30s), recovered to READY");
            } else {
                lastError_ = "Camera is not ready. Current state: " + devices::deviceStateToString(state_);
                lastUpdateTime_ = std::chrono::system_clock::now();
                return false;
            }
        } else if (state_ != devices::DeviceState::STATE_READY) {
            lastError_ = "Camera is not ready. Current state: " + devices::deviceStateToString(state_);
            lastUpdateTime_ = std::chrono::system_clock::now();
            return false;
        }

        if (!commandProcessor_ || !cameraModel_) {
            lastError_ = "Camera not initialized";
            lastUpdateTime_ = std::chrono::system_clock::now();
            return false;
        }
        
        // Track this capture
        {
            std::lock_guard<std::mutex> captureLock(captureMutex_);
            pendingCaptures_[captureId] = ""; // Will be set when download completes
        }
        
        // Enqueue take picture command
        auto takePictureCmd = std::make_shared<TakePictureCommand>(cameraModel_.get());
        commandProcessor_->enqueue(takePictureCmd);
        
        // Update state inline (do not call updateState() - it would lock stateMutex_ again and deadlock)
        devices::DeviceState oldState = state_;
        state_ = devices::DeviceState::STATE_PROCESSING;
        lastUpdateTime_ = std::chrono::system_clock::now();
        stateCb = stateChangedCallback_;
    }
    
    logging::Logger::getInstance().info("Capture command queued: " + captureId);
    logging::Logger::getInstance().info(
        "Camera state changed: " + devices::deviceStateToString(devices::DeviceState::STATE_READY) +
        " -> " + devices::deviceStateToString(devices::DeviceState::STATE_PROCESSING)
    );
    if (stateCb) {
        stateCb(devices::DeviceState::STATE_PROCESSING);
    }
    return true;
}

devices::DeviceState EdsdkCameraAdapter::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

bool EdsdkCameraAdapter::startPreview() {
    if (!commandProcessor_ || !cameraModel_) {
        logging::Logger::getInstance().warn("LiveView: camera not initialized");
        return false;
    }
    evfStartedPromise_ = std::promise<bool>();
    std::future<bool> evfFut = evfStartedPromise_.get_future();
    commandProcessor_->enqueue(std::make_shared<StartEvfCommand>(this));
    bool ok = false;
    try {
        ok = evfFut.get();
    } catch (...) {}
    if (!ok) return false;
    liveViewServer_.start(EdsdkLiveviewServer::DEFAULT_PORT);
    evfPumpRunning_ = true;
    pendingEvfFrames_ = 0;
    evfPumpThread_ = std::thread([this]() {
        while (evfPumpRunning_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            if (!evfPumpRunning_) break;
            while (evfPumpRunning_ && pendingEvfFrames_.load() > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (!evfPumpRunning_) break;
            if (commandProcessor_) {
                pendingEvfFrames_++;
                commandProcessor_->enqueue(std::make_shared<GetEvfFrameCommand>(this));
            }
        }
    });
    return true;
}

bool EdsdkCameraAdapter::stopPreview() {
    evfPumpRunning_ = false;
    if (evfPumpThread_.joinable())
        evfPumpThread_.join();
    if (commandProcessor_)
        commandProcessor_->enqueue(std::make_shared<StopEvfCommand>(this));
    liveViewServer_.stop();
    return true;
}

void EdsdkCameraAdapter::setEvfRefs(EdsStreamRef streamRef, EdsBaseRef evfImageRef) {
    releaseEvfRefs();
    evfStream_ = streamRef;
    evfImageRef_ = evfImageRef;
}

void EdsdkCameraAdapter::releaseEvfRefs() {
    if (evfImageRef_) {
        EdsRelease(evfImageRef_);
        evfImageRef_ = nullptr;
    }
    if (evfStream_) {
        EdsRelease(evfStream_);
        evfStream_ = nullptr;
    }
}

void EdsdkCameraAdapter::onEvfStarted(bool success) {
    try {
        evfStartedPromise_.set_value(success);
    } catch (...) {}
}

bool EdsdkCameraAdapter::setSettings(const devices::CameraSettings& settings) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    settings_ = settings;
    // TODO: Apply settings to camera via EDSDK
    return true;
}

devices::CameraSettings EdsdkCameraAdapter::getSettings() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return settings_;
}

void EdsdkCameraAdapter::setCaptureCompleteCallback(std::function<void(const devices::CaptureCompleteEvent&)> callback) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    captureCompleteCallback_ = callback;
}

void EdsdkCameraAdapter::setStateChangedCallback(std::function<void(devices::DeviceState)> callback) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    stateChangedCallback_ = callback;
}

int EdsdkCameraAdapter::pumpEvents(int maxCalls) {
    // Deprecated: EDSDK is now single-threaded (command processor only). Do not call from main thread.
    // The real event pump runs in EdsdkCommandProcessor::run() when the queue is empty.
    (void)maxCalls;
    return 0;
}

void EdsdkCameraAdapter::onSessionOpened() {
    // Handlers are registered in OpenSessionCommand (after EdsOpenSession, before SaveTo/Capacity).
    logging::Logger::getInstance().info("Camera session opened");
    updateState(devices::DeviceState::STATE_READY);
}

void EdsdkCameraAdapter::onSessionClosed() {
    logging::Logger::getInstance().info("Camera session closed");
    updateState(devices::DeviceState::DISCONNECTED);
}

void EdsdkCameraAdapter::onDownloadComplete(const std::string& filePath, const std::string& captureId) {
    logging::Logger::getInstance().info("onDownloadComplete: filePath=" + filePath + ", captureId=" + captureId);
    std::string imageIndex = std::filesystem::path(filePath).stem().string();

    devices::CaptureCompleteEvent event;
    event.captureId = captureId;
    event.filePath = filePath;
    event.imageIndex = imageIndex;
    event.imageFormat = "jpeg";
    event.width = 0;
    event.height = 0;

    std::vector<uint8_t> imageData = readImageFile(filePath);
    if (imageData.empty()) {
        logging::Logger::getInstance().warn("onDownloadComplete: readImageFile returned empty for " + filePath);
        event.success = false;
        event.errorMessage = "Failed to read image file";
        updateState(devices::DeviceState::STATE_READY);
        event.state = devices::DeviceState::STATE_READY;
        if (!captureCompleteCallback_) {
            logging::Logger::getInstance().warn("onDownloadComplete (failure path): captureCompleteCallback_ is NULL");
        } else {
            captureCompleteCallback_(event);
        }
        return;
    }
    event.imageData = std::move(imageData);
    event.success = true;

    {
        std::lock_guard<std::mutex> captureLock(captureMutex_);
        pendingCaptures_.erase(captureId);
    }

    event.state = devices::DeviceState::STATE_READY;
    // Send camera_capture_complete first so Flutter receives 촬영 완료 신호 (with exact filePath).
    // Then set READY so the next capture() is accepted.
    if (!captureCompleteCallback_) {
        logging::Logger::getInstance().warn("onDownloadComplete: captureCompleteCallback_ is NULL - CAMERA_CAPTURE_COMPLETE will not be sent");
    } else {
        captureCompleteCallback_(event);
    }
    updateState(devices::DeviceState::STATE_READY);
}

void EdsdkCameraAdapter::onError(EdsError error) {
    std::string errorMsg = "EDSDK error: " + std::to_string(error);
    setLastError(errorMsg);
    logging::Logger::getInstance().error(errorMsg);
    // DEVICE_BUSY(129)는 촬영 직후 일시적으로 올 수 있음. ERROR로 바꾸면 다음 촬영이 막힘.
    EdsError errId = (error & EDS_ERRORID_MASK);
    if (errId == EDS_ERR_DEVICE_BUSY) {
        logging::Logger::getInstance().warn("Ignoring DEVICE_BUSY in onError - keeping current state for next capture");
        return;
    }
    updateState(devices::DeviceState::STATE_ERROR);
}

void EdsdkCameraAdapter::onObjectEvent(EdsUInt32 event, EdsBaseRef ref) {
    // One download per image: only DirItemRequestTransfer (camera requests host to download).
    // DirItemCreated can also fire per image; handling both caused 5 shots -> 2 saved (duplicate handling consumed paths/pending).
    const bool useForDownload = (event == kEdsObjectEvent_DirItemRequestTransfer) && ref;
    if (!useForDownload) {
        if (ref) {
            EdsRelease(ref);
        }
        return;
    }

    std::string fullPath = config::ConfigManager::getInstance().getNextImagePath();
    std::string captureId;
    {
        std::lock_guard<std::mutex> captureLock(captureMutex_);
        if (!pendingCaptures_.empty()) {
            captureId = pendingCaptures_.begin()->first;
            pendingCaptures_.erase(pendingCaptures_.begin());
        }
    }
    if (fullPath.empty()) {
        logging::Logger::getInstance().warn("ObjectEvent: getNextImagePath failed");
        EdsRelease(ref);
        return;
    }

    if (captureId.empty()) {
        logging::Logger::getInstance().warn("ObjectEvent: No pending capture for new image (event " + std::to_string(event) + "), releasing ref");
        EdsRelease(ref);
        return;
    }

    auto downloadCmd = std::make_shared<DownloadCommand>(
        cameraModel_.get(),
        static_cast<EdsDirectoryItemRef>(ref),
        fullPath,
        captureId
    );

    if (commandProcessor_) {
        commandProcessor_->enqueue(downloadCmd);
    } else {
        EdsRelease(ref);
    }
}

void EdsdkCameraAdapter::updateState(devices::DeviceState newState) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ != newState) {
        devices::DeviceState oldState = state_;
        state_ = newState;
        lastUpdateTime_ = std::chrono::system_clock::now();
        
        logging::Logger::getInstance().info(
            "Camera state changed: " + devices::deviceStateToString(oldState) +
            " -> " + devices::deviceStateToString(newState)
        );
        
        if (stateChangedCallback_) {
            stateChangedCallback_(newState);
        }
    }
}

void EdsdkCameraAdapter::setLastError(const std::string& error) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastError_ = error;
    lastUpdateTime_ = std::chrono::system_clock::now();
}

std::vector<uint8_t> EdsdkCameraAdapter::readImageFile(const std::string& filePath) const {
    std::vector<uint8_t> data;
    
    try {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            logging::Logger::getInstance().error("Failed to open file: " + filePath);
            return data;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        data.resize(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
            logging::Logger::getInstance().error("Failed to read file: " + filePath);
            data.clear();
        }
        
        file.close();
    } catch (const std::exception& e) {
        logging::Logger::getInstance().error("Exception reading file: " + std::string(e.what()));
        data.clear();
    }
    
    return data;
}

} // namespace canon
