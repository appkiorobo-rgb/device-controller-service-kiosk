// include/vendor_adapters/canon/edsdk_forward_decls.h
#pragma once

// Forward declarations for EDSDK types (to avoid including EDSDK.h in headers)
// This prevents Windows SDK conflicts and type redefinition errors
// Actual EDSDK types will be defined in .cpp files via edsdk_wrapper.h
// 
// IMPORTANT: Include this header in all headers that need EDSDK types,
// but do NOT include edsdk_wrapper.h in headers - only in .cpp files

// Protect from Windows SDK conflicts BEFORE any includes
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

// Only define our forward declarations when EDSDK.h has NOT been included yet.
// When a .cpp includes edsdk_wrapper.h first, EDSDK_TYPES_ALREADY_DEFINED is set and we skip
// to avoid redefinition errors (our typedefs vs real EDSDK types).
#ifndef EDSDK_TYPES_ALREADY_DEFINED

// Forward declarations for EDSDK types
struct _EdsCameraRef;
typedef struct _EdsCameraRef* EdsCameraRef;

struct _EdsBaseRef;
typedef struct _EdsBaseRef* EdsBaseRef;

struct _EdsDirectoryItemRef;
typedef struct _EdsDirectoryItemRef* EdsDirectoryItemRef;

struct _EdsStreamRef;
typedef struct _EdsStreamRef* EdsStreamRef;

typedef unsigned int EdsUInt32;
typedef unsigned long long EdsUInt64;
typedef int EdsError;
typedef unsigned int EdsPropertyID;
typedef char EdsChar;
typedef void EdsVoid;  // EDSDK uses 'void', not 'void*'
typedef unsigned char EdsBool;
typedef unsigned int EdsDataType;

#endif // !EDSDK_TYPES_ALREADY_DEFINED

// EDSCALLBACK macro (needed for callback function declarations)
#ifndef EDSCALLBACK
    #define EDSCALLBACK __stdcall
#endif
