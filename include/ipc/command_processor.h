// include/ipc/command_processor.h
#pragma once

#include "ipc/message_types.h"
#include "service_core/device_orchestrator.h"
#include <unordered_map>
#include <mutex>
#include <memory>

namespace device_controller::ipc {

// CommandProcessor - handles idempotent command processing
// Part of IPC Layer
class CommandProcessor {
public:
    CommandProcessor(std::shared_ptr<DeviceOrchestrator> orchestrator);
    ~CommandProcessor();

    // Process command and return response
    // Implements idempotency: duplicate commandId returns cached response
    Response processCommand(const Command& command);

    // Clear command cache (for testing or admin purposes)
    void clearCache();

private:
    std::shared_ptr<DeviceOrchestrator> orchestrator_;
    std::mutex mutex_;
    
    // Idempotency cache: commandId -> Response
    std::unordered_map<std::string, Response> responseCache_;

    // Command handlers
    Response handleCameraCapture(const Command& command);
    Response handlePrinterPrint(const Command& command);
    Response handlePaymentStart(const Command& command);
    Response handleSnapshotRequest(const Command& command);
    Response handleUnknownCommand(const Command& command);

    // Helper to create error response
    Response createErrorResponse(const std::string& commandId, 
                                 const std::string& errorCode,
                                 const std::string& errorMessage);
};

} // namespace device_controller::ipc
