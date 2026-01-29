// include/vendor_adapters/canon/edsdk_wrapper.h
#pragma once

// Wrapper to include EDSDK.h safely without Windows SDK conflicts
// This should be included AFTER logger.h and device types, BEFORE other Windows headers

// Push current warning state
#pragma warning(push)
#pragma warning(disable: 4005) // macro redefinition
#pragma warning(disable: 4996) // deprecated functions

#ifdef _WIN32
    // Save current defines
    #ifndef EDSDK_WRAPPER_WIN32_LEAN_SAVED
        #ifdef WIN32_LEAN_AND_MEAN
            #define EDSDK_WRAPPER_WIN32_LEAN_SAVED
        #endif
    #endif
    
    #ifndef EDSDK_WRAPPER_NOMINMAX_SAVED
        #ifdef NOMINMAX
            #define EDSDK_WRAPPER_NOMINMAX_SAVED
        #endif
    #endif
    
    // Set Windows SDK protection
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

// Include EDSDK.h
#include "EDSDK.h"

// Mark that real EDSDK types are now defined (edsdk_forward_decls.h will skip its typedefs)
#define EDSDK_TYPES_ALREADY_DEFINED 1

// Restore warning state
#pragma warning(pop)
