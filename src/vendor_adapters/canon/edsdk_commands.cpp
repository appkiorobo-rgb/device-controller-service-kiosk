// src/vendor_adapters/canon/edsdk_commands.cpp
// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"

// Include device types BEFORE EDSDK to avoid conflicts
#include "devices/device_types.h"

// Include EDSDK wrapper (includes EDSDK.h safely) AFTER device types
#include "vendor_adapters/canon/edsdk_wrapper.h"

#include "vendor_adapters/canon/edsdk_commands.h"
#include "vendor_adapters/canon/edsdk_camera_model.h"
#include "vendor_adapters/canon/edsdk_camera_adapter.h"
#include "vendor_adapters/canon/edsdk_event_handler.h"
#include "config/config_manager.h"
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

// Maximum items to delete when flushing internal memory (safety limit)
static const EdsUInt32 kFlushInternalMaxItems = 500;

// Recursively delete all directory items under inRef (volume or folder).
// Always deletes index 0 and recurses into folders first (indices shift after delete).
static void deleteAllItemsInDirectory(EdsBaseRef inRef, EdsUInt32& deletedSoFar) {
    if (deletedSoFar >= kFlushInternalMaxItems)
        return;
    EdsUInt32 count = 0;
    EdsError err = EdsGetChildCount(inRef, &count);
    if (err != EDS_ERR_OK || count == 0)
        return;
    while (deletedSoFar < kFlushInternalMaxItems) {
        err = EdsGetChildCount(inRef, &count);
        if (err != EDS_ERR_OK || count == 0)
            break;
        EdsDirectoryItemRef itemRef = nullptr;
        err = EdsGetChildAtIndex(inRef, 0, &itemRef);
        if (err != EDS_ERR_OK || !itemRef)
            break;
        EdsDirectoryItemInfo info = {};
        err = EdsGetDirectoryItemInfo(itemRef, &info);
        if (err == EDS_ERR_OK && info.isFolder) {
            deleteAllItemsInDirectory(itemRef, deletedSoFar);
        }
        err = EdsDeleteDirectoryItem(itemRef);
        if (err == EDS_ERR_OK) {
            deletedSoFar++;
        }
    }
}

// Flush camera internal memory (volumes with kEdsStorageType_Non). When images
// remain in internal memory, SaveTo Host can fail with EDS_ERR_DEVICE_BUSY (129).
// Deleting those items allows SaveTo Host to succeed.
static void flushInternalMemoryVolumes(EdsCameraRef cameraRef) {
    EdsUInt32 volCount = 0;
    EdsError err = EdsGetChildCount(cameraRef, &volCount);
    if (err != EDS_ERR_OK || volCount == 0)
        return;
    for (EdsUInt32 v = 0; v < volCount; v++) {
        EdsVolumeRef volRef = nullptr;
        err = EdsGetChildAtIndex(cameraRef, v, &volRef);
        if (err != EDS_ERR_OK || !volRef)
            continue;
        EdsVolumeInfo volInfo = {};
        err = EdsGetVolumeInfo(volRef, &volInfo);
        if (err != EDS_ERR_OK) {
            EdsRelease(volRef);
            continue;
        }
        if (volInfo.storageType != kEdsStorageType_Non) {
            EdsRelease(volRef);
            continue;
        }
        EdsUInt32 deletedSoFar = 0;
        deleteAllItemsInDirectory(volRef, deletedSoFar);
        if (deletedSoFar > 0) {
            logging::Logger::getInstance().info(
                "InitializeCameraCommand: Flushed internal memory (volume type Non): deleted " +
                std::to_string(deletedSoFar) + " item(s)");
        }
        EdsRelease(volRef);
    }
}

// Map EDSDK error code to human-readable description (for logging)
static std::string edsdkErrorToString(EdsError err) {
    EdsError id = (err & EDS_ERRORID_MASK);
    switch (id) {
        case EDS_ERR_DEVICE_BUSY: return "Device busy (retry or wait)";
        case EDS_ERR_DEVICE_NOT_RELEASED: return "Device not released (shutter/Evf state)";
        case EDS_ERR_TAKE_PICTURE_AF_NG: return "Take picture AF failed (autofocus did not succeed)";
        case EDS_ERR_TAKE_PICTURE_NO_CARD_NG: return "No memory card";
        case EDS_ERR_TAKE_PICTURE_CARD_PROTECT_NG: return "Card write protected";
        case EDS_ERR_TAKE_PICTURE_LV_REL_PROHIBIT_MODE_NG: return "LiveView release prohibited (turn off Evf)";
        case EDS_ERR_DEVICE_INVALID: return "Device invalid";
        case EDS_ERR_DEVICE_NOT_FOUND: return "Device not found";
        default: break;
    }
    std::ostringstream os;
    os << "0x" << std::hex << std::setfill('0') << std::setw(4) << (id & 0xFFFF);
    return os.str();
}

// InitializeCameraCommand - all EDSDK init on command processor thread
InitializeCameraCommand::InitializeCameraCommand(canon::EdsdkCameraAdapter* adapter, const EdsdkInitCallbacks& callbacks)
    : EdsdkCommand(nullptr)
    , adapter_(adapter)
    , callbacks_(callbacks) {
}

bool InitializeCameraCommand::execute() {
    if (!adapter_) {
        logging::Logger::getInstance().error("InitializeCameraCommand: No adapter");
        return true;
    }

    // (1) EdsInitializeSDK on this thread
    EdsError err = EdsInitializeSDK();
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("InitializeCameraCommand: EdsInitializeSDK failed: " + std::to_string(err));
        adapter_->onInitComplete(false);
        return true;
    }
    adapter_->incrementSdkRefCount();
    logging::Logger::getInstance().info("InitializeCameraCommand: EDSDK initialized on command processor thread");

    EdsCameraListRef cameraList = nullptr;
    EdsCameraRef cameraRef = nullptr;
    EdsUInt32 count = 0;
    std::string deviceName = "Canon EOS Camera";

    err = EdsGetCameraList(&cameraList);
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("InitializeCameraCommand: EdsGetCameraList failed: " + std::to_string(err));
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }

    err = EdsGetChildCount(cameraList, &count);
    if (err != EDS_ERR_OK || count == 0) {
        logging::Logger::getInstance().error("InitializeCameraCommand: No cameras found");
        if (cameraList) EdsRelease(cameraList);
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }

    err = EdsGetChildAtIndex(cameraList, 0, &cameraRef);
    if (cameraList) {
        EdsRelease(cameraList);
        cameraList = nullptr;
    }
    if (err != EDS_ERR_OK || !cameraRef) {
        logging::Logger::getInstance().error("InitializeCameraCommand: EdsGetChildAtIndex failed: " + std::to_string(err));
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }

    EdsDeviceInfo deviceInfo;
    err = EdsGetDeviceInfo(cameraRef, &deviceInfo);
    if (err == EDS_ERR_OK) {
        deviceName = std::string(deviceInfo.szDeviceDescription);
        logging::Logger::getInstance().info("InitializeCameraCommand: Found camera: " + deviceName);
    }

    std::unique_ptr<canon::EdsdkCameraModel> model = std::make_unique<canon::EdsdkCameraModel>(cameraRef);
    cameraRef = nullptr;

    model->setSessionOpenedCallback(callbacks_.onSessionOpened);
    model->setSessionClosedCallback(callbacks_.onSessionClosed);
    model->setDownloadCompleteCallback(callbacks_.onDownloadComplete);
    model->setErrorCallback(callbacks_.onError);
    model->setObjectEventCallback(callbacks_.onObjectEvent);

    adapter_->setCameraModelAndDeviceName(std::move(model), deviceName);

    canon::EdsdkCameraModel* modelPtr = adapter_->getCameraModel();
    if (!modelPtr || !modelPtr->getCameraObject()) {
        logging::Logger::getInstance().error("InitializeCameraCommand: Model not set");
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }

    // Open session (same as OpenSessionCommand)
    err = EdsOpenSession(modelPtr->getCameraObject());
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("InitializeCameraCommand: EdsOpenSession failed: " + std::to_string(err));
        modelPtr->notifyError(err);
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }

    // Flush internal memory before SaveTo Host. When images remain in camera
    // internal memory, Set SaveTo can fail with EDS_ERR_DEVICE_BUSY (129).
    flushInternalMemoryVolumes(modelPtr->getCameraObject());

    EdsUInt32 saveTo = kEdsSaveTo_Host;
    err = EdsSetPropertyData(modelPtr->getCameraObject(), kEdsPropID_SaveTo, 0, sizeof(saveTo), &saveTo);
    if (err != EDS_ERR_OK) {
        if ((err & EDS_ERRORID_MASK) == EDS_ERR_DEVICE_BUSY) {
            logging::Logger::getInstance().info(
                "InitializeCameraCommand: SaveTo Host returned DEVICE_BUSY(129), flushing all volumes and retrying");
            EdsUInt32 volCount = 0;
            if (EdsGetChildCount(modelPtr->getCameraObject(), &volCount) == EDS_ERR_OK) {
                for (EdsUInt32 v = 0; v < volCount; v++) {
                    EdsVolumeRef volRef = nullptr;
                    if (EdsGetChildAtIndex(modelPtr->getCameraObject(), v, &volRef) != EDS_ERR_OK || !volRef)
                        continue;
                    EdsUInt32 deletedSoFar = 0;
                    deleteAllItemsInDirectory(volRef, deletedSoFar);
                    if (deletedSoFar > 0) {
                        logging::Logger::getInstance().info(
                            "InitializeCameraCommand: Flushed volume " + std::to_string(v) + ": deleted " +
                            std::to_string(deletedSoFar) + " item(s)");
                    }
                    EdsRelease(volRef);
                }
            }
            err = EdsSetPropertyData(modelPtr->getCameraObject(), kEdsPropID_SaveTo, 0, sizeof(saveTo), &saveTo);
        }
    }
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("InitializeCameraCommand: SaveTo Host FAILED: " + std::to_string(err) +
            " (" + edsdkErrorToString(err) + ")");
        modelPtr->notifyError(err);
        EdsCloseSession(modelPtr->getCameraObject());
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }
    logging::Logger::getInstance().info("InitializeCameraCommand: SaveTo Host set OK");

    bool locked = false;
    err = EdsSendStatusCommand(modelPtr->getCameraObject(), kEdsCameraStatusCommand_UILock, 0);
    if (err == EDS_ERR_OK) locked = true;

    EdsCapacity cap;
    cap.numberOfFreeClusters = 0x7FFFFFFF;
    cap.bytesPerSector = 512;
    cap.reset = 1;
    err = EdsSetCapacity(modelPtr->getCameraObject(), cap);
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("InitializeCameraCommand: SetCapacity FAILED: " + std::to_string(err));
        modelPtr->notifyError(err);
        if (locked) EdsSendStatusCommand(modelPtr->getCameraObject(), kEdsCameraStatusCommand_UIUnLock, 0);
        EdsCloseSession(modelPtr->getCameraObject());
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }
    logging::Logger::getInstance().info("InitializeCameraCommand: SetCapacity OK");

    if (locked) {
        EdsSendStatusCommand(modelPtr->getCameraObject(), kEdsCameraStatusCommand_UIUnLock, 0);
    }

    err = EdsSetObjectEventHandler(
        modelPtr->getCameraObject(),
        kEdsObjectEvent_All,
        canon::EdsdkEventHandler::handleObjectEvent,
        static_cast<EdsVoid*>(modelPtr)
    );
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("InitializeCameraCommand: ObjectEventHandler FAILED: " + std::to_string(err));
        modelPtr->notifyError(err);
        EdsCloseSession(modelPtr->getCameraObject());
        adapter_->decrementSdkRefCountAndMaybeTerminate();
        adapter_->onInitComplete(false);
        return true;
    }
    logging::Logger::getInstance().info("InitializeCameraCommand: ObjectEventHandler registered OK");

    err = EdsSetPropertyEventHandler(modelPtr->getCameraObject(), kEdsPropertyEvent_All,
        canon::EdsdkEventHandler::handlePropertyEvent, static_cast<EdsVoid*>(modelPtr));
    if (err == EDS_ERR_OK) {
        EdsSetCameraStateEventHandler(modelPtr->getCameraObject(), kEdsStateEvent_All,
            canon::EdsdkEventHandler::handleStateEvent, static_cast<EdsVoid*>(modelPtr));
    }

    logging::Logger::getInstance().info("InitializeCameraCommand: Session opened successfully");
    modelPtr->notifySessionOpened();
    adapter_->onInitComplete(true);
    return true;
}

// OpenSessionCommand (kept for compatibility; full init is in InitializeCameraCommand)
OpenSessionCommand::OpenSessionCommand(canon::EdsdkCameraModel* model)
    : EdsdkCommand(model) {
}

bool OpenSessionCommand::execute() {
    EdsError err = EDS_ERR_OK;
    bool locked = false;
    
    if (!model_ || !model_->getCameraObject()) {
        logging::Logger::getInstance().error("OpenSessionCommand: Invalid camera model");
        return true; // Don't retry
    }
    
    // Open session with camera
    err = EdsOpenSession(model_->getCameraObject());
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("OpenSessionCommand: EdsOpenSession failed: " + std::to_string(err));
        model_->notifyError(err);
        return true;
    }

    flushInternalMemoryVolumes(model_->getCameraObject());

    // (A) SaveTo Host - MUST succeed or transfer won't happen. Check return value.
    EdsUInt32 saveTo = kEdsSaveTo_Host;  // 1=Camera only, 2=Host(PC), 3=Both
    err = EdsSetPropertyData(model_->getCameraObject(), kEdsPropID_SaveTo, 0, sizeof(saveTo), &saveTo);
    if (err != EDS_ERR_OK && (err & EDS_ERRORID_MASK) == EDS_ERR_DEVICE_BUSY) {
        EdsUInt32 volCount = 0;
        if (EdsGetChildCount(model_->getCameraObject(), &volCount) == EDS_ERR_OK) {
            for (EdsUInt32 v = 0; v < volCount; v++) {
                EdsVolumeRef volRef = nullptr;
                if (EdsGetChildAtIndex(model_->getCameraObject(), v, &volRef) != EDS_ERR_OK || !volRef) continue;
                EdsUInt32 deletedSoFar = 0;
                deleteAllItemsInDirectory(volRef, deletedSoFar);
                EdsRelease(volRef);
            }
        }
        err = EdsSetPropertyData(model_->getCameraObject(), kEdsPropID_SaveTo, 0, sizeof(saveTo), &saveTo);
    }
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("OpenSessionCommand: SaveTo Host FAILED (required for transfer): " + std::to_string(err));
        model_->notifyError(err);
        EdsCloseSession(model_->getCameraObject());
        return true;
    }
    logging::Logger::getInstance().info("OpenSession: SaveTo Host set OK - images will transfer to host");
    
    // UI lock (optional)
    err = EdsSendStatusCommand(model_->getCameraObject(), kEdsCameraStatusCommand_UILock, 0);
    if (err == EDS_ERR_OK) {
        locked = true;
    }
    
    // (B) SetCapacity - required when saving to Host (SD 없이 Host 저장 시 필수). Check return value.
    EdsCapacity cap;
    cap.numberOfFreeClusters = 0x7FFFFFFF;
    cap.bytesPerSector = 512;
    cap.reset = 1;
    err = EdsSetCapacity(model_->getCameraObject(), cap);
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("OpenSessionCommand: SetCapacity FAILED (camera may refuse transfer): " + std::to_string(err));
        model_->notifyError(err);
        if (locked) {
            EdsSendStatusCommand(model_->getCameraObject(), kEdsCameraStatusCommand_UIUnLock, 0);
        }
        EdsCloseSession(model_->getCameraObject());
        return true;
    }
    logging::Logger::getInstance().info("OpenSession: SetCapacity OK (host capacity notified to camera)");
    
    // Unlock UI
    if (locked) {
        EdsSendStatusCommand(model_->getCameraObject(), kEdsCameraStatusCommand_UIUnLock, 0);
    }
    
    // (1) Handler registration AFTER SaveTo/Capacity (same thread). Many cameras never fire callbacks if registered before SaveTo.
    err = EdsSetObjectEventHandler(
        model_->getCameraObject(),
        kEdsObjectEvent_All,
        canon::EdsdkEventHandler::handleObjectEvent,
        static_cast<EdsVoid*>(model_)
    );
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("OpenSessionCommand: ObjectEventHandler registration FAILED: " + std::to_string(err));
        model_->notifyError(err);
        EdsCloseSession(model_->getCameraObject());
        return true;
    }
    logging::Logger::getInstance().info("OpenSession: ObjectEventHandler registered OK (after SaveTo/Capacity)");
    
    err = EdsSetPropertyEventHandler(
        model_->getCameraObject(),
        kEdsPropertyEvent_All,
        canon::EdsdkEventHandler::handlePropertyEvent,
        static_cast<EdsVoid*>(model_)
    );
    if (err == EDS_ERR_OK) {
        err = EdsSetCameraStateEventHandler(
            model_->getCameraObject(),
            kEdsStateEvent_All,
            canon::EdsdkEventHandler::handleStateEvent,
            static_cast<EdsVoid*>(model_)
        );
    }
    // Property/State handler failure is non-fatal for transfer
    
    logging::Logger::getInstance().info("Camera session opened successfully");
    model_->notifySessionOpened();
    return true;
}

// CloseSessionCommand
CloseSessionCommand::CloseSessionCommand(canon::EdsdkCameraModel* model, std::function<void()> onClosed)
    : EdsdkCommand(model)
    , onClosed_(std::move(onClosed)) {
}

bool CloseSessionCommand::execute() {
    if (!model_ || !model_->getCameraObject()) {
        if (onClosed_) onClosed_();
        return true;
    }

    EdsError err = EdsCloseSession(model_->getCameraObject());
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("CloseSessionCommand failed: " + std::to_string(err));
        model_->notifyError(err);
    } else {
        logging::Logger::getInstance().info("Camera session closed");
        model_->notifySessionClosed();
    }

    model_->releaseCameraRef();
    if (onClosed_) onClosed_();
    return true;
}

// TakePictureCommand
TakePictureCommand::TakePictureCommand(canon::EdsdkCameraModel* model)
    : EdsdkCommand(model) {
}

bool TakePictureCommand::execute() {
    if (!model_ || !model_->getCameraObject()) {
        logging::Logger::getInstance().error("TakePictureCommand: Invalid camera model");
        return true;
    }

    EdsCameraRef cam = model_->getCameraObject();

    // Recommended kiosk flow (EDSDK): Completely_NonAF — no AF trigger at shutter, use LiveView pre-focused state.
    const EdsUInt32 shutterCommand = kEdsCameraCommand_ShutterButton_Completely_NonAF;
    EdsError err = EdsSendCommand(cam, kEdsCameraCommand_PressShutterButton, shutterCommand);

    if (err == EDS_ERR_OK) {
        err = EdsSendCommand(cam, kEdsCameraCommand_PressShutterButton, kEdsCameraCommand_ShutterButton_OFF);
    }

    if (err != EDS_ERR_OK) {
        EdsError errId = (err & EDS_ERRORID_MASK);
        if (errId == EDS_ERR_DEVICE_BUSY) {
            logging::Logger::getInstance().warn("TakePictureCommand: Device busy, will retry");
            return false; // Retry
        }
        std::string desc = edsdkErrorToString(err);
        logging::Logger::getInstance().error("TakePictureCommand failed: " + std::to_string(err) + " (" + desc + ").");
        model_->notifyError(err);
        return true;
    }

    logging::Logger::getInstance().info("TakePictureCommand executed successfully (kiosk flow: NonAF, LiveView pre-focus).");
    return true;
}

// DownloadCommand
DownloadCommand::DownloadCommand(canon::EdsdkCameraModel* model, EdsDirectoryItemRef directoryItem, const std::string& savePath, const std::string& captureId)
    : EdsdkCommand(model)
    , directoryItem_(directoryItem)
    , savePath_(savePath)
    , captureId_(captureId) {
    // Add reference to prevent premature release
    if (directoryItem_) {
        EdsRetain(directoryItem_);
    }
}

DownloadCommand::~DownloadCommand() {
    if (directoryItem_) {
        EdsRelease(directoryItem_);
        directoryItem_ = nullptr;
    }
}

bool DownloadCommand::execute() {
    if (!model_ || !directoryItem_) {
        logging::Logger::getInstance().error("DownloadCommand: Invalid parameters");
        return true;
    }
    
    EdsError err = EDS_ERR_OK;
    EdsStreamRef stream = nullptr;
    
    // Get directory item info
    EdsDirectoryItemInfo dirItemInfo;
    err = EdsGetDirectoryItemInfo(directoryItem_, &dirItemInfo);
    
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("DownloadCommand: Failed to get directory item info: " + std::to_string(err));
        model_->notifyError(err);
        return true;
    }
    
    // savePath_ is full path: {base}/{sessionId}/{index}.jpg
    std::filesystem::path fullPathObj(savePath_);
    std::error_code ec;
    std::filesystem::create_directories(fullPathObj.parent_path(), ec);
    std::string fullPath = fullPathObj.string();
    
    // Create file stream
    if (err == EDS_ERR_OK) {
        err = EdsCreateFileStream(
            fullPath.c_str(),
            kEdsFileCreateDisposition_CreateAlways,
            kEdsAccess_ReadWrite,
            &stream
        );
    }
    
    // Set progress callback
    if (err == EDS_ERR_OK && stream) {
        err = EdsSetProgressCallback(stream, ProgressCallback, kEdsProgressOption_Periodically, this);
    }
    
    // Download image
    if (err == EDS_ERR_OK && stream) {
        err = EdsDownload(directoryItem_, dirItemInfo.size, stream);
    }
    
    // Complete download
    if (err == EDS_ERR_OK) {
        err = EdsDownloadComplete(directoryItem_);
    }
    
    // Release directory item
    if (directoryItem_) {
        EdsRelease(directoryItem_);
        directoryItem_ = nullptr;
    }
    
    // Release stream
    if (stream) {
        EdsRelease(stream);
        stream = nullptr;
    }
    
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("DownloadCommand failed: " + std::to_string(err));
        model_->notifyError(err);
        return true;
    }
    // Notify model about download completion
    model_->notifyDownloadComplete(fullPath, captureId_);
    
    return true;
}

EdsError EDSCALLBACK DownloadCommand::ProgressCallback(
    EdsUInt32 inPercent,
    EdsVoid* inContext,
    EdsBool* outCancel
) {
    if (outCancel) {
        // EDS_FALSE is typically 0, EDS_TRUE is typically 1
        *outCancel = 0;  // Don't cancel
    }
    
    // Could notify progress here if needed
    if (inPercent % 25 == 0) { // Log every 25%
        logging::Logger::getInstance().debug("Download progress: " + std::to_string(inPercent) + "%");
    }
    
    // EDS_ERR_OK should be defined in EDSDK.h
    return EDS_ERR_OK;
}

// GetPropertyCommand
GetPropertyCommand::GetPropertyCommand(canon::EdsdkCameraModel* model, EdsPropertyID propertyID)
    : EdsdkCommand(model)
    , propertyID_(propertyID) {
}

bool GetPropertyCommand::execute() {
    if (!model_ || !model_->getCameraObject()) {
        return true;
    }
    
    EdsError err = getProperty(propertyID_);
    
    if (err != EDS_ERR_OK) {
        if ((err & EDS_ERRORID_MASK) == EDS_ERR_DEVICE_BUSY) {
            return false; // Retry
        }
        model_->notifyError(err);
    }
    
    return true;
}

EdsError GetPropertyCommand::getProperty(EdsPropertyID propertyID) {
    EdsError err = EDS_ERR_OK;
    EdsDataType dataType = kEdsDataType_Unknown;
    EdsUInt32 dataSize = 0;
    
    // Get property size
    err = EdsGetPropertySize(
        model_->getCameraObject(),
        propertyID,
        0,
        &dataType,
        &dataSize
    );
    
    if (err != EDS_ERR_OK) {
        return err;
    }
    
    // Get property data based on type
    if (dataType == kEdsDataType_UInt32) {
        EdsUInt32 data;
        err = EdsGetPropertyData(
            model_->getCameraObject(),
            propertyID,
            0,
            dataSize,
            &data
        );
        
        if (err == EDS_ERR_OK) {
            model_->setPropertyUInt32(propertyID, data);
        }
    } else if (dataType == kEdsDataType_String) {
        EdsChar str[EDS_MAX_NAME];
        err = EdsGetPropertyData(
            model_->getCameraObject(),
            propertyID,
            0,
            dataSize,
            str
        );
        
        if (err == EDS_ERR_OK) {
            model_->setPropertyString(propertyID, str);
        }
    }
    
    return err;
}

// ----- LiveView (EVF) commands -----
StartEvfCommand::StartEvfCommand(canon::EdsdkCameraAdapter* adapter)
    : EdsdkCommand(adapter ? adapter->getCameraModel() : nullptr)
    , adapter_(adapter) {
}

bool StartEvfCommand::execute() {
    if (!adapter_ || !model_ || !model_->getCameraObject()) {
        if (adapter_) adapter_->onEvfStarted(false);
        return true;
    }
    EdsCameraRef cam = model_->getCameraObject();
    EdsUInt32 outDevice = kEdsEvfOutputDevice_PC;
    EdsError err = EdsSetPropertyData(cam, kEdsPropID_Evf_OutputDevice, 0, sizeof(outDevice), &outDevice);
    if (err != EDS_ERR_OK) {
        logging::Logger::getInstance().error("StartEvfCommand: Set Evf_OutputDevice PC failed: " + std::to_string(err));
        model_->notifyError(err);
        adapter_->onEvfStarted(false);
        return true;
    }
    EdsStreamRef streamRef = nullptr;
    err = EdsCreateMemoryStream(4 * 1024 * 1024, &streamRef);
    if (err != EDS_ERR_OK || !streamRef) {
        logging::Logger::getInstance().error("StartEvfCommand: EdsCreateMemoryStream failed: " + std::to_string(err));
        adapter_->onEvfStarted(false);
        return true;
    }
    EdsEvfImageRef evfImageRef = nullptr;
    err = EdsCreateEvfImageRef(streamRef, &evfImageRef);
    if (err != EDS_ERR_OK || !evfImageRef) {
        EdsRelease(streamRef);
        logging::Logger::getInstance().error("StartEvfCommand: EdsCreateEvfImageRef failed: " + std::to_string(err));
        adapter_->onEvfStarted(false);
        return true;
    }
    adapter_->setEvfRefs(streamRef, static_cast<EdsBaseRef>(evfImageRef));
    adapter_->onEvfStarted(true);
    return true;
}

GetEvfFrameCommand::GetEvfFrameCommand(canon::EdsdkCameraAdapter* adapter)
    : EdsdkCommand(adapter ? adapter->getCameraModel() : nullptr)
    , adapter_(adapter) {
}

bool GetEvfFrameCommand::execute() {
    if (!adapter_ || !model_ || !model_->getCameraObject()) {
        if (adapter_) adapter_->onEvfFrameProcessed();
        return true;
    }
    EdsStreamRef streamRef = adapter_->getEvfStream();
    EdsBaseRef evfImageRef = adapter_->getEvfImageRef();
    if (!streamRef || !evfImageRef) {
        adapter_->onEvfFrameProcessed();
        return true;
    }
    EdsError err = EdsDownloadEvfImage(model_->getCameraObject(), static_cast<EdsEvfImageRef>(evfImageRef));
    if (err != EDS_ERR_OK) {
        adapter_->onEvfFrameProcessed();
        return true;
    }
    EdsUInt64 len = 0;
    err = EdsGetLength(streamRef, &len);
    if (err != EDS_ERR_OK || len == 0 || len > 4 * 1024 * 1024) {
        adapter_->onEvfFrameProcessed();
        return true;
    }
    err = EdsSeek(streamRef, 0, kEdsSeek_Begin);
    if (err != EDS_ERR_OK) {
        adapter_->onEvfFrameProcessed();
        return true;
    }
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    EdsUInt64 readSize = 0;
    err = EdsRead(streamRef, len, buf.data(), &readSize);
    if (err != EDS_ERR_OK || readSize == 0) {
        adapter_->onEvfFrameProcessed();
        return true;
    }
    adapter_->getLiveViewServer()->setFrame(buf.data(), static_cast<size_t>(readSize));
    adapter_->onEvfFrameProcessed();
    return true;
}

StopEvfCommand::StopEvfCommand(canon::EdsdkCameraAdapter* adapter)
    : EdsdkCommand(adapter ? adapter->getCameraModel() : nullptr)
    , adapter_(adapter) {
}

bool StopEvfCommand::execute() {
    if (!adapter_ || !model_ || !model_->getCameraObject()) {
        if (adapter_) adapter_->releaseEvfRefs();
        return true;
    }
    EdsUInt32 outDevice = kEdsEvfOutputDevice_TFT;
    EdsSetPropertyData(model_->getCameraObject(), kEdsPropID_Evf_OutputDevice, 0, sizeof(outDevice), &outDevice);
    adapter_->releaseEvfRefs();
    return true;
}
