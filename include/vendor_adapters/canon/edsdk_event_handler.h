// include/vendor_adapters/canon/edsdk_event_handler.h
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

#include <functional>

// Forward declaration
namespace canon {
    class EdsdkCameraModel;
}

namespace canon {

// EDSDK event handler callbacks
class EdsdkEventHandler {
public:
    // Object event callback (for download requests, etc.)
    static EdsError EDSCALLBACK handleObjectEvent(
        EdsUInt32 inEvent,
        EdsBaseRef inRef,
        EdsVoid* inContext
    );
    
    // Property event callback (for property changes)
    static EdsError EDSCALLBACK handlePropertyEvent(
        EdsUInt32 inEvent,
        EdsUInt32 inPropertyID,
        EdsUInt32 inParam,
        EdsVoid* inContext
    );
    
    // State event callback (for camera connection/disconnection)
    static EdsError EDSCALLBACK handleStateEvent(
        EdsUInt32 inEvent,
        EdsUInt32 inParam,
        EdsVoid* inContext
    );
    
private:
    static void fireObjectEvent(canon::EdsdkCameraModel* model, EdsUInt32 event, EdsBaseRef ref);
    static void firePropertyEvent(canon::EdsdkCameraModel* model, EdsUInt32 event, EdsUInt32 propertyID);
    static void fireStateEvent(canon::EdsdkCameraModel* model, EdsUInt32 event);
};

} // namespace canon
