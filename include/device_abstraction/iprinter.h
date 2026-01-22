// include/device_abstraction/iprinter.h
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>

namespace device_controller {

// Printer state machine
enum class PrinterState {
    DISCONNECTED,
    CONNECTING,
    READY,
    PRINTING,
    ERROR,
    HUNG,
    PAPER_OUT,
    JAMMED
};

// Printer event types
enum class PrinterEventType {
    STATE_CHANGED,
    PRINT_COMPLETE,
    PRINT_FAILED,
    ERROR_OCCURRED,
    PAPER_OUT,
    JAM_DETECTED
};

// Printer event data
struct PrinterEvent {
    PrinterEventType type;
    PrinterState state;
    std::string errorCode;
    std::string errorMessage;
    std::string jobId;
    std::chrono::milliseconds timestamp;
};

// Event callback type
using PrinterEventCallback = std::function<void(const PrinterEvent&)>;

// IPrinter interface - stable abstraction for printer devices
class IPrinter {
public:
    virtual ~IPrinter() = default;

    // Get current state
    virtual PrinterState getState() const = 0;

    // Initialize printer connection
    // Returns true if initialization started, false if already initialized or error
    virtual bool initialize() = 0;

    // Shutdown printer connection
    virtual void shutdown() = 0;

    // Start print job
    // imagePath: path to image file to print
    // Does not return success/failure directly - result comes via event callback
    // Returns true if print started, false if rejected
    virtual bool startPrint(const std::string& imagePath) = 0;

    // Cancel ongoing print job
    virtual void cancelPrint() = 0;

    // Register event callback
    virtual void setEventCallback(PrinterEventCallback callback) = 0;

    // Get device information
    virtual std::string getDeviceId() const = 0;
    virtual std::string getDeviceName() const = 0;
};

} // namespace device_controller
