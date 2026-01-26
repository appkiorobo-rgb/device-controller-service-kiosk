// src/ipc/command_processor.cpp
#include "ipc/command_processor.h"
#include "common/uuid_generator.h"
#include "ipc/message_types.h"
#include "device_abstraction/icamera.h"
#include "device_abstraction/iprinter.h"
#include "device_abstraction/ipayment_terminal.h"
#include <sstream>
#include <chrono>

namespace device_controller::ipc {

CommandProcessor::CommandProcessor(std::shared_ptr<DeviceOrchestrator> orchestrator)
    : orchestrator_(orchestrator)
{
}

CommandProcessor::~CommandProcessor() {
}

Response CommandProcessor::processCommand(const Command& command) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Idempotency check: if commandId already processed, return cached response
    auto it = responseCache_.find(command.commandId);
    if (it != responseCache_.end()) {
        return it->second;
    }

    // Process new command
    Response response;
    response.commandId = command.commandId;
    response.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (command.type == "camera_capture") {
        response = handleCameraCapture(command);
    } else if (command.type == "printer_print") {
        response = handlePrinterPrint(command);
    } else if (command.type == "payment_start") {
        response = handlePaymentStart(command);
    } else if (command.type == "payment_cancel") {
        response = handlePaymentCancel(command);
    } else if (command.type == "payment_status_check") {
        response = handlePaymentStatusCheck(command);
    } else if (command.type == "payment_reset") {
        response = handlePaymentReset(command);
    } else if (command.type == "payment_device_check") {
        response = handlePaymentDeviceCheck(command);
    } else if (command.type == "snapshot_request") {
        response = handleSnapshotRequest(command);
    } else {
        response = handleUnknownCommand(command);
    }

    // Cache response for idempotency
    responseCache_[command.commandId] = response;

    return response;
}

void CommandProcessor::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    responseCache_.clear();
}

Response CommandProcessor::handleCameraCapture(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto camera = orchestrator_->getCamera();
        if (!camera) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No camera available");
        }

        if (camera->getState() != CameraState::READY) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_READY", 
                "Camera is not in ready state");
        }

        if (!camera->startCapture()) {
            return createErrorResponse(command.commandId, "COMMAND_REJECTED", 
                "Camera capture rejected");
        }

        response.result = {
            {"deviceId", camera->getDeviceId()},
            {"state", static_cast<int>(camera->getState())}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handlePrinterPrint(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto printer = orchestrator_->getPrinter();
        if (!printer) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No printer available");
        }

        if (printer->getState() != PrinterState::READY) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_READY", 
                "Printer is not in ready state");
        }

        std::string imagePath = command.payload.value("imagePath", "");
        if (imagePath.empty()) {
            return createErrorResponse(command.commandId, "INVALID_PAYLOAD", 
                "imagePath is required");
        }

        if (!printer->startPrint(imagePath)) {
            return createErrorResponse(command.commandId, "COMMAND_REJECTED", 
                "Print command rejected");
        }

        response.result = {
            {"deviceId", printer->getDeviceId()},
            {"state", static_cast<int>(printer->getState())}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handlePaymentStart(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto terminal = orchestrator_->getPaymentTerminal();
        if (!terminal) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No payment terminal available");
        }

        if (terminal->getState() != PaymentTerminalState::READY) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_READY", 
                "Payment terminal is not in ready state");
        }

        int64_t amount = command.payload.value("amount", 0LL);
        if (amount <= 0) {
            return createErrorResponse(command.commandId, "INVALID_PAYLOAD", 
                "amount must be greater than 0");
        }

        if (!terminal->startPayment(amount)) {
            return createErrorResponse(command.commandId, "COMMAND_REJECTED", 
                "Payment command rejected");
        }

        response.result = {
            {"deviceId", terminal->getDeviceId()},
            {"state", static_cast<int>(terminal->getState())}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handlePaymentCancel(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto terminal = orchestrator_->getPaymentTerminal();
        if (!terminal) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No payment terminal available");
        }

        terminal->cancelPayment();

        response.result = {
            {"deviceId", terminal->getDeviceId()},
            {"state", static_cast<int>(terminal->getState())}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handlePaymentStatusCheck(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto terminal = orchestrator_->getPaymentTerminal();
        if (!terminal) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No payment terminal available");
        }

        response.result = {
            {"deviceId", terminal->getDeviceId()},
            {"state", static_cast<int>(terminal->getState())},
            {"deviceName", terminal->getDeviceName()}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handlePaymentReset(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto terminal = orchestrator_->getPaymentTerminal();
        if (!terminal) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No payment terminal available");
        }

        if (!terminal->reset()) {
            return createErrorResponse(command.commandId, "RESET_FAILED", 
                "Failed to reset payment terminal");
        }

        response.result = {
            {"deviceId", terminal->getDeviceId()},
            {"state", static_cast<int>(terminal->getState())}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handlePaymentDeviceCheck(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        auto terminal = orchestrator_->getPaymentTerminal();
        if (!terminal) {
            return createErrorResponse(command.commandId, "DEVICE_NOT_FOUND", "No payment terminal available");
        }

        // Device check is vendor-specific, so we'll just return current state
        response.result = {
            {"deviceId", terminal->getDeviceId()},
            {"state", static_cast<int>(terminal->getState())},
            {"deviceName", terminal->getDeviceName()}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handleSnapshotRequest(const Command& command) {
    Response response;
    response.commandId = command.commandId;
    response.status = STATUS_OK;

    try {
        std::vector<std::string> deviceTypes;
        if (command.payload.contains("deviceTypes") && command.payload["deviceTypes"].is_array()) {
            for (const auto& dt : command.payload["deviceTypes"]) {
                deviceTypes.push_back(dt.get<std::string>());
            }
        }

        auto snapshot = orchestrator_->getStateSnapshot(deviceTypes);
        response.result = {
            {"snapshot", snapshot}
        };
    } catch (const std::exception& e) {
        return createErrorResponse(command.commandId, "PROCESSING_ERROR", e.what());
    }

    return response;
}

Response CommandProcessor::handleUnknownCommand(const Command& command) {
    return createErrorResponse(command.commandId, "UNKNOWN_COMMAND", 
        "Unknown command type: " + command.type);
}

Response CommandProcessor::createErrorResponse(const std::string& commandId, 
                                               const std::string& errorCode,
                                               const std::string& errorMessage) {
    Response response;
    response.commandId = commandId;
    response.status = STATUS_FAILED;
    response.error = {
        {"code", errorCode},
        {"message", errorMessage}
    };
    response.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

} // namespace device_controller::ipc
