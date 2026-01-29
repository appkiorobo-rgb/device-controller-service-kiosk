// include/vendor_adapters/canon/edsdk_commands.h
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
#include <memory>

// Forward declarations
namespace canon {
    class EdsdkCameraModel;
    class EdsdkCameraAdapter;
}

// Callbacks for InitializeCameraCommand (set on model from EDSDK thread)
struct EdsdkInitCallbacks {
    std::function<void()> onSessionOpened;
    std::function<void()> onSessionClosed;
    std::function<void(const std::string&, const std::string&)> onDownloadComplete;
    std::function<void(EdsError)> onError;
    std::function<void(EdsUInt32, EdsBaseRef)> onObjectEvent;
};

// Base command class
class EdsdkCommand {
public:
    virtual ~EdsdkCommand() = default;
    virtual bool execute() = 0;
    
protected:
    canon::EdsdkCameraModel* model_;
    
    EdsdkCommand(canon::EdsdkCameraModel* model) : model_(model) {}
};

// Initialize EDSDK + discover camera + open session (all on command processor thread)
class InitializeCameraCommand : public EdsdkCommand {
public:
    InitializeCameraCommand(canon::EdsdkCameraAdapter* adapter, const EdsdkInitCallbacks& callbacks);
    bool execute() override;
private:
    canon::EdsdkCameraAdapter* adapter_;
    EdsdkInitCallbacks callbacks_;
};

// Open session command (used only after camera ref is obtained; open is done inside InitializeCameraCommand)
class OpenSessionCommand : public EdsdkCommand {
public:
    OpenSessionCommand(canon::EdsdkCameraModel* model);
    bool execute() override;
};

// Close session + release camera ref + optional SDK terminate (all on command processor thread)
class CloseSessionCommand : public EdsdkCommand {
public:
    CloseSessionCommand(canon::EdsdkCameraModel* model, std::function<void()> onClosed = nullptr);
    bool execute() override;
private:
    std::function<void()> onClosed_;
};

// Take picture command
class TakePictureCommand : public EdsdkCommand {
public:
    TakePictureCommand(canon::EdsdkCameraModel* model);
    bool execute() override;
};

// Download command - downloads image and saves to local file
class DownloadCommand : public EdsdkCommand {
public:
    DownloadCommand(canon::EdsdkCameraModel* model, EdsDirectoryItemRef directoryItem, const std::string& savePath, const std::string& captureId);
    ~DownloadCommand();
    bool execute() override;
    
private:
    EdsDirectoryItemRef directoryItem_;
    std::string savePath_;
    std::string captureId_;
    
    static EdsError EDSCALLBACK ProgressCallback(
        EdsUInt32 inPercent,
        EdsVoid* inContext,
        EdsBool* outCancel
    );
};

// Get property command
class GetPropertyCommand : public EdsdkCommand {
public:
    GetPropertyCommand(canon::EdsdkCameraModel* model, EdsPropertyID propertyID);
    bool execute() override;
    
private:
    EdsPropertyID propertyID_;
    EdsError getProperty(EdsPropertyID propertyID);
};
