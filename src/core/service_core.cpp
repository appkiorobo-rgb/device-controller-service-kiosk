// src/core/service_core.cpp
// logger.h? ?? include?? Windows SDK ?? ??
#include "logging/logger.h"
#include "core/service_core.h"
#include "devices/device_types.h"
#include "ipc/message_types.h"
#include "ipc/message_parser.h"
#include "vendor_adapters/smartro/smartro_payment_adapter.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>
#include <map>

namespace core {

ServiceCore::ServiceCore()
    : ipcServer_(deviceManager_)
    , running_(false)
    , taskQueueRunning_(false) {
}

ServiceCore::~ServiceCore() {
    stop();
}

bool ServiceCore::start() {
    // Register command handlers
    registerCommandHandlers();
    
    // Start task worker thread (for reset/device check only)
    startTaskWorker();
    
    // Setup client connected callback - perform system status check
    ipcServer_.getPipeServer().setClientConnectedCallback([this]() {
        logging::Logger::getInstance().info("Client connected - performing system status check");
        performSystemStatusCheck();
    });
    
    // Setup client disconnected callback - cancel payment when client disconnects
    ipcServer_.getPipeServer().setClientDisconnectedCallback([this]() {
        logging::Logger::getInstance().info("Client disconnected - cancelling any ongoing payment");
        auto terminal = deviceManager_.getDefaultPaymentTerminal();
        if (terminal) {
            auto info = terminal->getDeviceInfo();
            if (info.state == devices::DeviceState::PROCESSING) {
                terminal->cancelPayment();
                logging::Logger::getInstance().info("Payment cancelled due to client disconnection");
            }
        }
    });
    
    // Start IPC server
    if (!ipcServer_.start()) {
        logging::Logger::getInstance().error("Failed to start IPC server");
        stopTaskWorker();
        return false;
    }
    
    // Setup event callbacks
    setupEventCallbacks();
    
    running_ = true;
    logging::Logger::getInstance().info("Service Core started successfully");
    return true;
}

void ServiceCore::stop() {
    stopTaskWorker();
    ipcServer_.stop();
    running_ = false;
    logging::Logger::getInstance().info("Service Core stopped");
}

void ServiceCore::registerCommandHandlers() {
    // Get state snapshot
    ipcServer_.registerHandler(ipc::CommandType::GET_STATE_SNAPSHOT, [this](const ipc::Command& cmd) {
        return handleGetStateSnapshot(cmd);
    });
    
    // Get device list
    ipcServer_.registerHandler(ipc::CommandType::GET_DEVICE_LIST, [this](const ipc::Command& cmd) {
        return handleGetDeviceList(cmd);
    });
    
    // Payment terminal commands
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_START, [this](const ipc::Command& cmd) {
        return handlePaymentStart(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_CANCEL, [this](const ipc::Command& cmd) {
        return handlePaymentCancel(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_TRANSACTION_CANCEL, [this](const ipc::Command& cmd) {
        return handlePaymentTransactionCancel(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_STATUS, [this](const ipc::Command& cmd) {
        return handlePaymentStatusCheck(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_RESET, [this](const ipc::Command& cmd) {
        return handlePaymentReset(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_DEVICE_CHECK, [this](const ipc::Command& cmd) {
        return handlePaymentDeviceCheck(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_CARD_UID_READ, [this](const ipc::Command& cmd) {
        return handlePaymentCardUidRead(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_LAST_APPROVAL, [this](const ipc::Command& cmd) {
        return handlePaymentLastApproval(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_IC_CARD_CHECK, [this](const ipc::Command& cmd) {
        return handlePaymentIcCardCheck(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::PAYMENT_SCREEN_SOUND_SETTING, [this](const ipc::Command& cmd) {
        return handlePaymentScreenSoundSetting(cmd);
    });
}

ipc::Response ServiceCore::handleGetStateSnapshot(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Collect all device information
    auto devices = deviceManager_.getAllDeviceInfo();
    
    for (const auto& device : devices) {
        resp.result[device.deviceId + ".deviceType"] = devices::deviceTypeToString(device.deviceType);
        resp.result[device.deviceId + ".deviceName"] = device.deviceName;
        resp.result[device.deviceId + ".state"] = std::to_string(static_cast<int>(device.state));
        resp.result[device.deviceId + ".stateString"] = devices::deviceStateToString(device.state);
        resp.result[device.deviceId + ".lastError"] = device.lastError;
    }
    
    return resp;
}

ipc::Response ServiceCore::handleGetDeviceList(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Device list by type
    auto paymentIds = deviceManager_.getDeviceIds(devices::DeviceType::PAYMENT_TERMINAL);
    auto printerIds = deviceManager_.getDeviceIds(devices::DeviceType::PRINTER);
    auto cameraIds = deviceManager_.getDeviceIds(devices::DeviceType::CAMERA);
    
    std::ostringstream oss;
    oss << "payment:";
    for (size_t i = 0; i < paymentIds.size(); ++i) {
        if (i > 0) oss << ",";
        oss << paymentIds[i];
    }
    resp.result["payment"] = oss.str();
    
    oss.str("");
    oss << "printer:";
    for (size_t i = 0; i < printerIds.size(); ++i) {
        if (i > 0) oss << ",";
        oss << printerIds[i];
    }
    resp.result["printer"] = oss.str();
    
    oss.str("");
    oss << "camera:";
    for (size_t i = 0; i < cameraIds.size(); ++i) {
        if (i > 0) oss << ",";
        oss << cameraIds[i];
    }
    resp.result["camera"] = oss.str();
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentStart(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment start command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Validate payload
    auto it = cmd.payload.find("amount");
    if (it == cmd.payload.end()) {
        logging::Logger::getInstance().warn("Payment start failed: Missing 'amount' parameter");
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PAYLOAD";
        error->message = "Missing 'amount' parameter";
        resp.error = error;
        return resp;
    }
    
    // Validate device exists
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        logging::Logger::getInstance().warn("Payment start failed: No payment terminal registered");
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    // Execute immediately - no queue needed, response is handled by background thread
    uint32_t amount = std::stoul(it->second);
    logging::Logger::getInstance().info("Executing payment start immediately: " + cmd.commandId + ", amount: " + it->second);
    
    if (terminal->startPayment(amount)) {
        resp.status = ipc::ResponseStatus::OK;
        auto info = terminal->getDeviceInfo();
        resp.result["commandId"] = cmd.commandId;
        resp.result["deviceId"] = info.deviceId;
        resp.result["state"] = std::to_string(static_cast<int>(info.state));
        resp.result["stateString"] = devices::deviceStateToString(info.state);
        logging::Logger::getInstance().info("Payment start command sent successfully");
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto info = terminal->getDeviceInfo();
        auto error = std::make_shared<ipc::Error>();
        error->code = "PAYMENT_START_FAILED";
        error->message = info.lastError;
        resp.error = error;
        logging::Logger::getInstance().error("Payment start failed: " + info.lastError);
    }
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentCancel(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment cancel command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Validate device exists
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        logging::Logger::getInstance().warn("Payment cancel failed: No payment terminal registered");
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    // Execute immediately - no queue needed
    logging::Logger::getInstance().info("Executing payment cancel immediately: " + cmd.commandId);
    
    if (terminal->cancelPayment()) {
        resp.status = ipc::ResponseStatus::OK;
        auto info = terminal->getDeviceInfo();
        resp.result["commandId"] = cmd.commandId;
        resp.result["deviceId"] = info.deviceId;
        resp.result["state"] = std::to_string(static_cast<int>(info.state));
        resp.result["stateString"] = devices::deviceStateToString(info.state);
        logging::Logger::getInstance().info("Payment cancel command sent successfully");
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto info = terminal->getDeviceInfo();
        auto error = std::make_shared<ipc::Error>();
        error->code = "PAYMENT_CANCEL_FAILED";
        error->message = info.lastError;
        resp.error = error;
        logging::Logger::getInstance().error("Payment cancel failed: " + info.lastError);
    }
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentStatusCheck(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    auto info = terminal->getDeviceInfo();
    resp.result["deviceId"] = info.deviceId;
    resp.result["state"] = std::to_string(static_cast<int>(info.state));
    resp.result["stateString"] = devices::deviceStateToString(info.state);
    resp.result["deviceName"] = info.deviceName;
    resp.result["lastError"] = info.lastError;
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentReset(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Validate device exists
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    // Execute immediately
    if (terminal->reset()) {
        resp.status = ipc::ResponseStatus::OK;
        auto info = terminal->getDeviceInfo();
        resp.result["commandId"] = cmd.commandId;
        resp.result["deviceId"] = info.deviceId;
        resp.result["state"] = std::to_string(static_cast<int>(info.state));
        resp.result["stateString"] = devices::deviceStateToString(info.state);
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto info = terminal->getDeviceInfo();
        auto error = std::make_shared<ipc::Error>();
        error->code = "PAYMENT_RESET_FAILED";
        error->message = info.lastError;
        resp.error = error;
    }
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentDeviceCheck(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Validate device exists
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    // Execute immediately
    if (terminal->checkDevice()) {
        resp.status = ipc::ResponseStatus::OK;
        auto info = terminal->getDeviceInfo();
        resp.result["commandId"] = cmd.commandId;
        resp.result["deviceId"] = info.deviceId;
        resp.result["state"] = std::to_string(static_cast<int>(info.state));
        resp.result["stateString"] = devices::deviceStateToString(info.state);
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto info = terminal->getDeviceInfo();
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_CHECK_FAILED";
        error->message = info.lastError;
        resp.error = error;
    }
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentCardUidRead(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment card UID read command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    // Cast to SmartroPaymentAdapter
    auto smartroAdapter = std::dynamic_pointer_cast<smartro::SmartroPaymentAdapter>(terminal);
    if (!smartroAdapter) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_DEVICE_TYPE";
        error->message = "Payment terminal is not a Smartro device";
        resp.error = error;
        return resp;
    }
    
    smartro::CardUidReadResponse cardResponse;
    if (!smartroAdapter->readCardUid(cardResponse)) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "CARD_UID_READ_FAILED";
        error->message = "Failed to read card UID";
        resp.error = error;
        return resp;
    }
    
    resp.status = ipc::ResponseStatus::OK;
    std::string uidHex;
    for (size_t i = 0; i < cardResponse.uid.size(); ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", cardResponse.uid[i]);
        uidHex += hex;
        if (i < cardResponse.uid.size() - 1) uidHex += " ";
    }
    resp.result["uid"] = uidHex;
    resp.result["uidLength"] = std::to_string(cardResponse.uid.size());
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentLastApproval(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment last approval command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    auto smartroAdapter = std::dynamic_pointer_cast<smartro::SmartroPaymentAdapter>(terminal);
    if (!smartroAdapter) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_DEVICE_TYPE";
        error->message = "Payment terminal is not a Smartro device";
        resp.error = error;
        return resp;
    }
    
    smartro::LastApprovalResponse lastApproval;
    if (!smartroAdapter->getLastApproval(lastApproval)) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "LAST_APPROVAL_FAILED";
        error->message = "Failed to get last approval";
        resp.error = error;
        return resp;
    }
    
    resp.status = ipc::ResponseStatus::OK;
    // LastApprovalResponse contains raw data (157 bytes, same as PaymentApprovalResponse)
    // Parse it if needed, or return raw data
    std::string dataHex;
    for (size_t i = 0; i < lastApproval.data.size(); ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", lastApproval.data[i]);
        dataHex += hex;
    }
    resp.result["data"] = dataHex;
    resp.result["dataLength"] = std::to_string(lastApproval.data.size());
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentIcCardCheck(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment IC card check command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    auto smartroAdapter = std::dynamic_pointer_cast<smartro::SmartroPaymentAdapter>(terminal);
    if (!smartroAdapter) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_DEVICE_TYPE";
        error->message = "Payment terminal is not a Smartro device";
        resp.error = error;
        return resp;
    }
    
    smartro::IcCardCheckResponse icResponse;
    if (!smartroAdapter->checkIcCard(icResponse)) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "IC_CARD_CHECK_FAILED";
        error->message = "Failed to check IC card";
        resp.error = error;
        return resp;
    }
    
    resp.status = ipc::ResponseStatus::OK;
    resp.result["cardStatus"] = std::string(1, icResponse.cardStatus);
    resp.result["cardInserted"] = (icResponse.cardStatus == 'O') ? "true" : "false";
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentScreenSoundSetting(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment screen/sound setting command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    // Extract parameters
    auto screenBrightnessIt = cmd.payload.find("screenBrightness");
    auto soundVolumeIt = cmd.payload.find("soundVolume");
    auto touchSoundVolumeIt = cmd.payload.find("touchSoundVolume");
    
    if (screenBrightnessIt == cmd.payload.end() || soundVolumeIt == cmd.payload.end() || touchSoundVolumeIt == cmd.payload.end()) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PAYLOAD";
        error->message = "Missing required parameters: screenBrightness, soundVolume, touchSoundVolume";
        resp.error = error;
        return resp;
    }
    
    auto smartroAdapter = std::dynamic_pointer_cast<smartro::SmartroPaymentAdapter>(terminal);
    if (!smartroAdapter) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_DEVICE_TYPE";
        error->message = "Payment terminal is not a Smartro device";
        resp.error = error;
        return resp;
    }
    
    smartro::ScreenSoundSettingRequest request;
    try {
        request.screenBrightness = static_cast<uint8_t>(std::stoi(screenBrightnessIt->second));
        request.soundVolume = static_cast<uint8_t>(std::stoi(soundVolumeIt->second));
        request.touchSoundVolume = static_cast<uint8_t>(std::stoi(touchSoundVolumeIt->second));
    } catch (const std::exception& e) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PARAMETER";
        error->message = "Invalid parameter format: " + std::string(e.what());
        resp.error = error;
        return resp;
    }
    
    smartro::ScreenSoundSettingResponse settingResponse;
    if (!smartroAdapter->setScreenSound(request, settingResponse)) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "SCREEN_SOUND_SETTING_FAILED";
        error->message = "Failed to set screen/sound";
        resp.error = error;
        return resp;
    }
    
    resp.status = ipc::ResponseStatus::OK;
    resp.result["screenBrightness"] = std::to_string(settingResponse.screenBrightness);
    resp.result["soundVolume"] = std::to_string(settingResponse.soundVolume);
    resp.result["touchSoundVolume"] = std::to_string(settingResponse.touchSoundVolume);
    
    return resp;
}

ipc::Response ServiceCore::handlePaymentTransactionCancel(const ipc::Command& cmd) {
    logging::Logger::getInstance().info("Received payment transaction cancel command: " + cmd.commandId);
    
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No payment terminal registered";
        resp.error = error;
        return resp;
    }
    
    auto smartroAdapter = std::dynamic_pointer_cast<smartro::SmartroPaymentAdapter>(terminal);
    if (!smartroAdapter) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_DEVICE_TYPE";
        error->message = "Payment terminal is not a Smartro device";
        resp.error = error;
        return resp;
    }
    
    // Extract parameters
    auto cancelTypeIt = cmd.payload.find("cancelType");
    auto transactionTypeIt = cmd.payload.find("transactionType");
    auto amountIt = cmd.payload.find("amount");
    auto approvalNumberIt = cmd.payload.find("approvalNumber");
    auto originalDateIt = cmd.payload.find("originalDate");
    auto originalTimeIt = cmd.payload.find("originalTime");
    auto taxIt = cmd.payload.find("tax");
    auto serviceIt = cmd.payload.find("service");
    auto installmentsIt = cmd.payload.find("installments");
    auto additionalInfoIt = cmd.payload.find("additionalInfo");
    
    if (cancelTypeIt == cmd.payload.end() || transactionTypeIt == cmd.payload.end() || 
        amountIt == cmd.payload.end() || approvalNumberIt == cmd.payload.end() ||
        originalDateIt == cmd.payload.end() || originalTimeIt == cmd.payload.end()) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PAYLOAD";
        error->message = "Missing required parameters: cancelType, transactionType, amount, approvalNumber, originalDate, originalTime";
        resp.error = error;
        return resp;
    }
    
    smartro::TransactionCancelRequest cancelRequest;
    try {
        cancelRequest.cancelType = cancelTypeIt->second[0];  // '1' or '2'
        cancelRequest.transactionType = static_cast<uint8_t>(std::stoi(transactionTypeIt->second));
        cancelRequest.amount = std::stoul(amountIt->second);
        cancelRequest.approvalNumber = approvalNumberIt->second;
        cancelRequest.originalDate = originalDateIt->second;
        cancelRequest.originalTime = originalTimeIt->second;
        cancelRequest.tax = (taxIt != cmd.payload.end()) ? std::stoul(taxIt->second) : 0;
        cancelRequest.service = (serviceIt != cmd.payload.end()) ? std::stoul(serviceIt->second) : 0;
        cancelRequest.installments = (installmentsIt != cmd.payload.end()) ? static_cast<uint8_t>(std::stoi(installmentsIt->second)) : 0;
        cancelRequest.additionalInfo = (additionalInfoIt != cmd.payload.end()) ? additionalInfoIt->second : "";
    } catch (const std::exception& e) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PARAMETER";
        error->message = "Invalid parameter format: " + std::string(e.what());
        resp.error = error;
        return resp;
    }
    
    smartro::TransactionCancelResponse cancelResponse;
    if (!smartroAdapter->cancelTransaction(cancelRequest, cancelResponse)) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "TRANSACTION_CANCEL_FAILED";
        error->message = "Failed to cancel transaction";
        resp.error = error;
        return resp;
    }
    
    resp.status = ipc::ResponseStatus::OK;
    resp.result["transactionType"] = std::string(1, cancelResponse.transactionType);
    resp.result["transactionMedium"] = std::string(1, cancelResponse.transactionMedium);
    resp.result["cardNumber"] = cancelResponse.cardNumber;
    resp.result["approvalAmount"] = cancelResponse.approvalAmount;
    resp.result["approvalNumber"] = cancelResponse.approvalNumber;
    resp.result["salesDate"] = cancelResponse.salesDate;
    resp.result["salesTime"] = cancelResponse.salesTime;
    resp.result["transactionId"] = cancelResponse.transactionId;
    resp.result["isRejected"] = cancelResponse.isRejected() ? "true" : "false";
    resp.result["isSuccess"] = cancelResponse.isSuccess() ? "true" : "false";
    
    return resp;
}

void ServiceCore::setupEventCallbacks() {
    // Setup payment terminal event callbacks (for IPC event broadcasting)
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (terminal) {
        terminal->setPaymentCompleteCallback([this](const devices::PaymentCompleteEvent& event) {
            publishPaymentCompleteEvent(event);
        });
        
        terminal->setPaymentFailedCallback([this](const devices::PaymentFailedEvent& event) {
            publishPaymentFailedEvent(event);
        });
        
        terminal->setPaymentCancelledCallback([this](const devices::PaymentCancelledEvent& event) {
            publishPaymentCancelledEvent(event);
        });
        
        terminal->setStateChangedCallback([this](devices::DeviceState state) {
            publishDeviceStateChangedEvent("payment", state);
        });
    }
}

void ServiceCore::publishPaymentCompleteEvent(const devices::PaymentCompleteEvent& event) {
    logging::Logger::getInstance().info("=== Publishing PAYMENT_COMPLETE event ===");
    logging::Logger::getInstance().info("Transaction ID: " + event.transactionId);
    logging::Logger::getInstance().info("Amount: " + std::to_string(event.amount));
    
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::PAYMENT_COMPLETE;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = "payment";
    
    ipcEvent.data["transactionId"] = event.transactionId;
    ipcEvent.data["amount"] = std::to_string(event.amount);
    ipcEvent.data["cardNumber"] = event.cardNumber;
    ipcEvent.data["approvalNumber"] = event.approvalNumber;
    ipcEvent.data["salesDate"] = event.salesDate;
    ipcEvent.data["salesTime"] = event.salesTime;
    ipcEvent.data["transactionMedium"] = event.transactionMedium;
    ipcEvent.data["state"] = std::to_string(static_cast<int>(event.state));
    
    logging::Logger::getInstance().info("Broadcasting PAYMENT_COMPLETE event to IPC clients");
    ipcServer_.broadcastEvent(ipcEvent);
    logging::Logger::getInstance().info("PAYMENT_COMPLETE event broadcasted");
}

void ServiceCore::publishPaymentFailedEvent(const devices::PaymentFailedEvent& event) {
    logging::Logger::getInstance().info("=== Publishing PAYMENT_FAILED event ===");
    logging::Logger::getInstance().info("Error Code: " + event.errorCode);
    logging::Logger::getInstance().info("Error Message: " + event.errorMessage);
    
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::PAYMENT_FAILED;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = "payment";
    
    ipcEvent.data["errorCode"] = event.errorCode;
    ipcEvent.data["errorMessage"] = event.errorMessage;
    ipcEvent.data["amount"] = std::to_string(event.amount);
    ipcEvent.data["state"] = std::to_string(static_cast<int>(event.state));
    
    logging::Logger::getInstance().info("Broadcasting PAYMENT_FAILED event to IPC clients");
    ipcServer_.broadcastEvent(ipcEvent);
    logging::Logger::getInstance().info("PAYMENT_FAILED event broadcasted");
}

void ServiceCore::publishPaymentCancelledEvent(const devices::PaymentCancelledEvent& event) {
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::PAYMENT_CANCELLED;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = "payment";
    
    ipcEvent.data["state"] = std::to_string(static_cast<int>(event.state));
    
    ipcServer_.broadcastEvent(ipcEvent);
}

void ServiceCore::publishDeviceStateChangedEvent(const std::string& deviceType, devices::DeviceState state) {
    logging::Logger::getInstance().info("=== Publishing DEVICE_STATE_CHANGED event ===");
    logging::Logger::getInstance().info("Device Type: " + deviceType + ", State: " + devices::deviceStateToString(state));
    
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::DEVICE_STATE_CHANGED;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = deviceType;
    
    ipcEvent.data["state"] = std::to_string(static_cast<int>(state));
    ipcEvent.data["stateString"] = devices::deviceStateToString(state);
    
    logging::Logger::getInstance().info("Broadcasting DEVICE_STATE_CHANGED event to IPC clients");
    ipcServer_.broadcastEvent(ipcEvent);
    logging::Logger::getInstance().info("DEVICE_STATE_CHANGED event broadcasted");
}

void ServiceCore::performSystemStatusCheck() {
    logging::Logger::getInstance().info("=== Starting system status check ===");
    
    std::map<std::string, devices::DeviceInfo> deviceStatuses;
    bool allHealthy = true;
    
    // Check all payment terminals
    auto paymentIds = deviceManager_.getDeviceIds(devices::DeviceType::PAYMENT_TERMINAL);
    for (const auto& deviceId : paymentIds) {
        auto terminal = deviceManager_.getPaymentTerminal(deviceId);
        if (terminal) {
            auto info = terminal->getDeviceInfo();
            logging::Logger::getInstance().info("Checking payment terminal: " + deviceId + ", state: " + devices::deviceStateToString(info.state));
            
            // If payment terminal is in PROCESSING state, cancel and recheck
            if (info.state == devices::DeviceState::PROCESSING) {
                logging::Logger::getInstance().warn("Payment terminal " + deviceId + " is in PROCESSING state - cancelling payment");
                terminal->cancelPayment();
                
                // Wait a bit for cancellation to complete
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                // Recheck status
                info = terminal->getDeviceInfo();
                logging::Logger::getInstance().info("Payment terminal " + deviceId + " status after cancel: " + devices::deviceStateToString(info.state));
            }
            
            // Perform device check
            logging::Logger::getInstance().info("Performing device check for: " + deviceId);
            if (!terminal->checkDevice()) {
                logging::Logger::getInstance().error("Device check failed for payment terminal: " + deviceId);
                allHealthy = false;
            } else {
                info = terminal->getDeviceInfo();
                logging::Logger::getInstance().info("Device check completed for " + deviceId + ", final state: " + devices::deviceStateToString(info.state));
            }
            
            deviceStatuses[deviceId] = info;
            
            // Check if device is in error state
            if (info.state == devices::DeviceState::ERROR || info.state == devices::DeviceState::DISCONNECTED) {
                allHealthy = false;
            }
        }
    }
    
    // Check all printers (future)
    auto printerIds = deviceManager_.getDeviceIds(devices::DeviceType::PRINTER);
    for (const auto& deviceId : printerIds) {
        auto printer = deviceManager_.getPrinter(deviceId);
        if (printer) {
            auto info = printer->getDeviceInfo();
            logging::Logger::getInstance().info("Checking printer: " + deviceId + ", state: " + devices::deviceStateToString(info.state));
            deviceStatuses[deviceId] = info;
            
            if (info.state == devices::DeviceState::ERROR || info.state == devices::DeviceState::DISCONNECTED) {
                allHealthy = false;
            }
        }
    }
    
    // Check all cameras (future)
    auto cameraIds = deviceManager_.getDeviceIds(devices::DeviceType::CAMERA);
    for (const auto& deviceId : cameraIds) {
        auto camera = deviceManager_.getCamera(deviceId);
        if (camera) {
            auto info = camera->getDeviceInfo();
            logging::Logger::getInstance().info("Checking camera: " + deviceId + ", state: " + devices::deviceStateToString(info.state));
            deviceStatuses[deviceId] = info;
            
            if (info.state == devices::DeviceState::ERROR || info.state == devices::DeviceState::DISCONNECTED) {
                allHealthy = false;
            }
        }
    }
    
    logging::Logger::getInstance().info("=== System status check completed - All healthy: " + std::string(allHealthy ? "YES" : "NO") + " ===");
    
    // Publish status check event
    publishSystemStatusCheckEvent(deviceStatuses, allHealthy);
}

void ServiceCore::publishSystemStatusCheckEvent(const std::map<std::string, devices::DeviceInfo>& deviceStatuses, bool allHealthy) {
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::SYSTEM_STATUS_CHECK;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = "system";
    
    ipcEvent.data["allHealthy"] = allHealthy ? "true" : "false";
    ipcEvent.data["deviceCount"] = std::to_string(deviceStatuses.size());
    
    // Add device statuses
    int index = 0;
    for (const auto& [deviceId, info] : deviceStatuses) {
        std::string prefix = "devices[" + std::to_string(index) + "].";
        ipcEvent.data[prefix + "deviceId"] = deviceId;
        ipcEvent.data[prefix + "deviceType"] = devices::deviceTypeToString(info.deviceType);
        ipcEvent.data[prefix + "deviceName"] = info.deviceName;
        ipcEvent.data[prefix + "state"] = std::to_string(static_cast<int>(info.state));
        ipcEvent.data[prefix + "stateString"] = devices::deviceStateToString(info.state);
        ipcEvent.data[prefix + "lastError"] = info.lastError;
        index++;
    }
    
    ipcServer_.broadcastEvent(ipcEvent);
}

std::string ServiceCore::generateUUID() {
    // Simple UUID generation
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 32; ++i) {
        oss << dis(gen);
        if (i == 7 || i == 11 || i == 15 || i == 19) {
            oss << "-";
        }
    }
    return oss.str();
}

void ServiceCore::startTaskWorker() {
    if (taskQueueRunning_) {
        return;
    }
    
    taskQueueRunning_ = true;
    taskWorkerThread_ = std::thread(&ServiceCore::taskWorkerThread, this);
    logging::Logger::getInstance().info("Task worker thread started");
}

void ServiceCore::stopTaskWorker() {
    if (!taskQueueRunning_) {
        return;
    }
    
    taskQueueRunning_ = false;
    taskQueueCondition_.notify_all();
    
    if (taskWorkerThread_.joinable()) {
        taskWorkerThread_.join();
    }
    
    logging::Logger::getInstance().info("Task worker thread stopped");
}

void ServiceCore::taskWorkerThread() {
    logging::Logger::getInstance().info("Task worker thread running");
    
    while (taskQueueRunning_ || !taskQueue_.empty()) {
        DeviceTask task;
        bool hasTask = false;
        
        {
            std::unique_lock<std::mutex> lock(taskQueueMutex_);
            
            // Wait for task or stop signal
            logging::Logger::getInstance().debug("Task worker waiting for task... (queue size: " + std::to_string(taskQueue_.size()) + ")");
            taskQueueCondition_.wait(lock, [this] {
                return !taskQueue_.empty() || !taskQueueRunning_;
            });
            
            if (!taskQueueRunning_ && taskQueue_.empty()) {
                logging::Logger::getInstance().debug("Task worker stopping: queue empty and not running");
                break;
            }
            
            if (!taskQueue_.empty()) {
                task = taskQueue_.front();
                taskQueue_.pop();
                hasTask = true;
                logging::Logger::getInstance().debug("Task worker picked task: " + task.commandId + ", remaining: " + std::to_string(taskQueue_.size()));
            }
        }
        
        if (!hasTask) {
            continue;
        }
        
        // Execute task
        try {
            logging::Logger::getInstance().info("Task worker executing task: " + task.commandId);
            switch (task.type) {
                case DeviceTask::Type::PAYMENT_START:
                    executePaymentStart(task);
                    break;
                case DeviceTask::Type::PAYMENT_CANCEL:
                    executePaymentCancel(task);
                    break;
                case DeviceTask::Type::PAYMENT_RESET:
                    executePaymentReset(task);
                    break;
                case DeviceTask::Type::PAYMENT_DEVICE_CHECK:
                    executePaymentDeviceCheck(task);
                    break;
                default:
                    logging::Logger::getInstance().warn("Unknown task type in worker thread");
                    break;
            }
            logging::Logger::getInstance().info("Task worker completed task: " + task.commandId);
        } catch (const std::exception& e) {
            logging::Logger::getInstance().error("Error executing task: " + std::string(e.what()));
        }
    }
    
    logging::Logger::getInstance().info("Task worker thread exiting");
}

void ServiceCore::enqueueTask(const DeviceTask& task) {
    {
        std::lock_guard<std::mutex> lock(taskQueueMutex_);
        taskQueue_.push(task);
        logging::Logger::getInstance().debug("Task queued: " + task.commandId + ", queue size: " + std::to_string(taskQueue_.size()));
    }
    taskQueueCondition_.notify_one();
    logging::Logger::getInstance().debug("Task queue condition notified");
}

void ServiceCore::executePaymentStart(const DeviceTask& task) {
    logging::Logger::getInstance().info("=== Executing payment start task: " + task.commandId + " ===");
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        logging::Logger::getInstance().error("Payment start failed: Payment terminal not found for task: " + task.commandId);
        return;
    }
    
    auto it = task.params.find("amount");
    if (it == task.params.end()) {
        logging::Logger::getInstance().error("Payment start failed: Missing amount parameter in task");
        return;
    }
    
    uint32_t amount = std::stoul(it->second);
    logging::Logger::getInstance().info("Calling terminal->startPayment(" + std::to_string(amount) + ")...");
    
    // Execute payment start (non-blocking - uses async API)
    bool result = terminal->startPayment(amount);
    
    if (!result) {
        auto info = terminal->getDeviceInfo();
        logging::Logger::getInstance().error("Payment start failed: " + info.lastError);
        // Error will be published via event callback
    } else {
        logging::Logger::getInstance().info("Payment start command sent successfully to device");
    }
    
    logging::Logger::getInstance().info("=== Payment start task completed: " + task.commandId + " ===");
}

void ServiceCore::executePaymentCancel(const DeviceTask& task) {
    logging::Logger::getInstance().info("=== Executing payment cancel task: " + task.commandId + " ===");
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        logging::Logger::getInstance().error("Payment cancel failed: Payment terminal not found for task: " + task.commandId);
        return;
    }
    
    // Execute payment cancel (non-blocking)
    logging::Logger::getInstance().info("Calling terminal->cancelPayment()...");
    bool result = terminal->cancelPayment();
    
    if (!result) {
        auto info = terminal->getDeviceInfo();
        logging::Logger::getInstance().error("Payment cancel failed: " + info.lastError);
        // Error will be published via event callback
    } else {
        logging::Logger::getInstance().info("Payment cancel command sent successfully to device");
    }
    
    logging::Logger::getInstance().info("=== Payment cancel task completed: " + task.commandId + " ===");
}

void ServiceCore::executePaymentReset(const DeviceTask& task) {
    logging::Logger::getInstance().info("Executing payment reset task: " + task.commandId);
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        logging::Logger::getInstance().error("Payment terminal not found for task: " + task.commandId);
        return;
    }
    
    // Execute payment reset
    if (!terminal->reset()) {
        logging::Logger::getInstance().error("Payment reset failed: " + terminal->getDeviceInfo().lastError);
    } else {
        logging::Logger::getInstance().info("Payment reset command sent successfully");
    }
}

void ServiceCore::executePaymentDeviceCheck(const DeviceTask& task) {
    logging::Logger::getInstance().info("Executing payment device check task: " + task.commandId);
    
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (!terminal) {
        logging::Logger::getInstance().error("Payment terminal not found for task: " + task.commandId);
        return;
    }
    
    // Execute device check
    if (!terminal->checkDevice()) {
        logging::Logger::getInstance().error("Device check failed: " + terminal->getDeviceInfo().lastError);
    } else {
        logging::Logger::getInstance().info("Device check completed successfully");
    }
}

} // namespace core
