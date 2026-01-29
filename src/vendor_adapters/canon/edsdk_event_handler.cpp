// src/vendor_adapters/canon/edsdk_event_handler.cpp
// Include logger first to prevent Windows SDK conflicts
#include "logging/logger.h"

// Include device types BEFORE EDSDK to avoid conflicts
#include "devices/device_types.h"

// Include EDSDK wrapper (includes EDSDK.h safely) AFTER device types
#include "vendor_adapters/canon/edsdk_wrapper.h"

#include "vendor_adapters/canon/edsdk_event_handler.h"
#include "vendor_adapters/canon/edsdk_camera_model.h"
#include <string>

namespace canon {

EdsError EDSCALLBACK EdsdkEventHandler::handleObjectEvent(
    EdsUInt32 inEvent,
    EdsBaseRef inRef,
    EdsVoid* inContext
) {
    canon::EdsdkCameraModel* model = static_cast<canon::EdsdkCameraModel*>(inContext);
    
    if (!model) {
        if (inRef) {
            EdsRelease(inRef);
        }
        return EDS_ERR_OK;
    }
    
    switch (inEvent) {
        case kEdsObjectEvent_DirItemRequestTransfer:
            // Image is ready for download
            logging::Logger::getInstance().info("ObjectEvent: DirItemRequestTransfer");
            fireObjectEvent(model, inEvent, inRef);
            // Don't release ref here - it will be handled by DownloadCommand
            break;
            
        case kEdsObjectEvent_DirItemCreated:
            // Some cameras use DirItemCreated (not RequestTransfer) for host download; do not release ref - adapter may use it
            logging::Logger::getInstance().info("ObjectEvent: DirItemCreated");
            fireObjectEvent(model, inEvent, inRef);
            break;
            
        case kEdsObjectEvent_DirItemRemoved:
            logging::Logger::getInstance().info("ObjectEvent: DirItemRemoved");
            fireObjectEvent(model, inEvent, inRef);
            if (inRef) {
                EdsRelease(inRef);
            }
            break;
            
        default:
            // Log any other object event (to confirm EdsGetEvent is dispatching)
            logging::Logger::getInstance().info("ObjectEvent: received event=0x" + std::to_string(inEvent) + " (not DirItemRequestTransfer/DirItemCreated)");
            if (inRef) {
                EdsRelease(inRef);
            }
            break;
    }
    
    return EDS_ERR_OK;
}

EdsError EDSCALLBACK EdsdkEventHandler::handlePropertyEvent(
    EdsUInt32 inEvent,
    EdsUInt32 inPropertyID,
    EdsUInt32 inParam,
    EdsVoid* inContext
) {
    canon::EdsdkCameraModel* model = static_cast<canon::EdsdkCameraModel*>(inContext);
    
    if (!model) {
        return EDS_ERR_OK;
    }
    
    switch (inEvent) {
        case kEdsPropertyEvent_PropertyChanged:
            logging::Logger::getInstance().debug("PropertyEvent: PropertyChanged - " + std::to_string(inPropertyID));
            firePropertyEvent(model, inEvent, inPropertyID);
            break;
            
        case kEdsPropertyEvent_PropertyDescChanged:
            logging::Logger::getInstance().debug("PropertyEvent: PropertyDescChanged - " + std::to_string(inPropertyID));
            firePropertyEvent(model, inEvent, inPropertyID);
            break;
    }
    
    return EDS_ERR_OK;
}

EdsError EDSCALLBACK EdsdkEventHandler::handleStateEvent(
    EdsUInt32 inEvent,
    EdsUInt32 inParam,
    EdsVoid* inContext
) {
    canon::EdsdkCameraModel* model = static_cast<canon::EdsdkCameraModel*>(inContext);
    
    if (!model) {
        return EDS_ERR_OK;
    }
    
    switch (inEvent) {
        case kEdsStateEvent_Shutdown:
            logging::Logger::getInstance().warn("StateEvent: Camera shutdown");
            fireStateEvent(model, inEvent);
            break;
            
        case kEdsStateEvent_WillSoonShutDown:
            logging::Logger::getInstance().info("StateEvent: Camera will soon shutdown");
            fireStateEvent(model, inEvent);
            break;
            
        default:
            break;
    }
    
    return EDS_ERR_OK;
}

void EdsdkEventHandler::fireObjectEvent(canon::EdsdkCameraModel* model, EdsUInt32 event, EdsBaseRef ref) {
    if (model) {
        model->notifyObjectEvent(event, ref);
    }
}

void EdsdkEventHandler::firePropertyEvent(canon::EdsdkCameraModel* model, EdsUInt32 event, EdsUInt32 propertyID) {
    // Property events are handled by the adapter if needed
    // For now, just log
}

void EdsdkEventHandler::fireStateEvent(canon::EdsdkCameraModel* model, EdsUInt32 event) {
    // State events are handled by the adapter
    // For now, just log
}

} // namespace canon
