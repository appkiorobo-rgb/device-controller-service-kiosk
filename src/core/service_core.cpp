// src/core/service_core.cpp
// Include message_types.h FIRST, before any header that pulls in Windows (e.g. edsdk).
// Windows macros (ERROR, result, response, Response) would otherwise corrupt ipc::Response/Event.
#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#ifdef result
#undef result
#endif
#ifdef response
#undef response
#endif
#ifdef Response
#undef Response
#endif
#endif

#include "ipc/message_types.h"
#include <sstream>
#include "logging/logger.h"
#include "core/service_core.h"
#include "config/config_manager.h"
#include "devices/device_types.h"
#include "devices/icamera.h"
#include "vendor_adapters/canon/edsdk_camera_adapter.h"
#include "ipc/message_parser.h"
#include "vendor_adapters/smartro/smartro_payment_adapter.h"
#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include "vendor_adapters/windows/windows_gdi_printer_adapter.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#include <iomanip>
#include <thread>
#include <map>
#include <vector>

// Undef Windows macros again after includes (edsdk, serial_port, etc. may pull winerror.h)
#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#ifdef result
#undef result
#endif
#ifdef response
#undef response
#endif
#ifdef Response
#undef Response
#endif
#endif

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
    
    // No automatic system status check on connect; client requests get_state_snapshot or detect_hardware when needed (avoids duplicate probe + 0-client broadcasts).

    // Setup client disconnected callback - reset all: cancel payment, stop liveview, etc.
    ipcServer_.getPipeServer().setClientDisconnectedCallback([this]() {
        logging::Logger::getInstance().info("Pipe disconnected - resetting (payment cancel, stop liveview)");
        resetOnClientDisconnect();
    });
    
    // Start IPC server
    if (!ipcServer_.start()) {
        logging::Logger::getInstance().error("Failed to start IPC server");
        stopTaskWorker();
        return false;
    }

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

    // Config (admin)
    ipcServer_.registerHandler(ipc::CommandType::GET_CONFIG, [this](const ipc::Command& cmd) {
        return handleGetConfig(cmd);
    });
    ipcServer_.registerHandler(ipc::CommandType::SET_CONFIG, [this](const ipc::Command& cmd) {
        return handleSetConfig(cmd);
    });
    ipcServer_.registerHandler(ipc::CommandType::PRINTER_PRINT, [this](const ipc::Command& cmd) {
        return handlePrinterPrint(cmd);
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
    
    // Camera commands
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_CAPTURE, [this](const ipc::Command& cmd) {
        return handleCameraCapture(cmd);
    });
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_SET_SESSION, [this](const ipc::Command& cmd) {
        return handleCameraSetSession(cmd);
    });
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_STATUS, [this](const ipc::Command& cmd) {
        return handleCameraStatus(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_START_PREVIEW, [this](const ipc::Command& cmd) {
        return handleCameraStartPreview(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_STOP_PREVIEW, [this](const ipc::Command& cmd) {
        return handleCameraStopPreview(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_SET_SETTINGS, [this](const ipc::Command& cmd) {
        return handleCameraSetSettings(cmd);
    });
    
    ipcServer_.registerHandler(ipc::CommandType::CAMERA_RECONNECT, [this](const ipc::Command& cmd) {
        return handleCameraReconnect(cmd);
    });

    // Detect Hardware: probe=true(또는 생략)일 때만 재연결 시도. probe=false면 현재 상태만 수집 (빠름).
    ipcServer_.registerHandler(ipc::CommandType::DETECT_HARDWARE, [this](const ipc::Command& cmd) {
        auto it = cmd.payload.find("probe");
        bool doProbe = (it == cmd.payload.end() || it->second != "false");
        if (doProbe) {
            tryReconnectDevicesBeforeDetect();
        }
        return handleDetectHardware(cmd);
    });
    ipcServer_.registerHandler(ipc::CommandType::GET_AVAILABLE_PRINTERS, [this](const ipc::Command& cmd) {
        return handleGetAvailablePrinters(cmd);
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

    // Collect all device information (fast; no probe)
    auto devices = deviceManager_.getAllDeviceInfo();

    bool anyNotReady = false;
    for (const auto& device : devices) {
        if (device.state != devices::DeviceState::STATE_READY) {
            anyNotReady = true;
        }
        resp.responseMap[device.deviceId + ".deviceType"] = devices::deviceTypeToString(device.deviceType);
        resp.responseMap[device.deviceId + ".deviceName"] = device.deviceName;
        resp.responseMap[device.deviceId + ".state"] = std::to_string(static_cast<int>(device.state));
        resp.responseMap[device.deviceId + ".stateString"] = devices::deviceStateToString(device.state);
        resp.responseMap[device.deviceId + ".lastError"] = device.lastError;
    }

    // READY가 아닌 장치가 있으면 백그라운드에서 재연결 시도 (응답은 즉시 반환)
    if (anyNotReady) {
        std::thread([this]() {
            try {
                logging::Logger::getInstance().info("State snapshot had non-READY device(s), starting background reconnect");
                tryReconnectDevicesBeforeDetect();
            } catch (const std::exception& e) {
                logging::Logger::getInstance().error("Background reconnect failed: " + std::string(e.what()));
            }
        }).detach();
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
    resp.responseMap["payment"] = oss.str();
    
    oss.str("");
    oss << "printer:";
    for (size_t i = 0; i < printerIds.size(); ++i) {
        if (i > 0) oss << ",";
        oss << printerIds[i];
    }
    resp.responseMap["printer"] = oss.str();
    
    oss.str("");
    oss << "camera:";
    for (size_t i = 0; i < cameraIds.size(); ++i) {
        if (i > 0) oss << ",";
        oss << cameraIds[i];
    }
    resp.responseMap["camera"] = oss.str();
    
    return resp;
}

ipc::Response ServiceCore::handleGetConfig(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto all = config::ConfigManager::getInstance().getAll();
    for (const auto& kv : all) {
        resp.responseMap[kv.first] = kv.second;
    }
    return resp;
}

ipc::Response ServiceCore::handleGetAvailablePrinters(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::vector<std::string> names = windows::WindowsGdiPrinterAdapter::getAvailablePrinterNames();
    std::ostringstream oss;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << names[i];
    }
    resp.responseMap["available_printers"] = oss.str();
    return resp;
}

ipc::Response ServiceCore::handleSetConfig(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    try {
        config::ConfigManager::getInstance().setFromMap(cmd.payload);
        config::ConfigManager::getInstance().saveIfInitialized();
        auto it = cmd.payload.find("printer.name");
        if (it != cmd.payload.end()) {
            auto printer = deviceManager_.getDefaultPrinter();
            auto* gdi = dynamic_cast<windows::WindowsGdiPrinterAdapter*>(printer.get());
            if (gdi) gdi->setPrinterName(it->second);
        }
        resp.status = ipc::ResponseStatus::OK;
        resp.responseMap["restart_required"] = "0";
    } catch (const std::exception& e) {
        resp.status = ipc::ResponseStatus::FAILED;
        auto err = std::make_shared<ipc::Error>();
        err->code = "CONFIG_SAVE_FAILED";
        err->message = e.what();
        resp.error = err;
    }
    return resp;
}

namespace {
    bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
        static const std::string kChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        out.clear();
        std::vector<int> T(256, -1);
        for (size_t i = 0; i < kChars.size(); ++i) T[static_cast<unsigned char>(kChars[i])] = static_cast<int>(i);
        int val = 0, valb = -8;
        for (unsigned char c : in) {
            if (T[c] == -1) break;
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return !out.empty() || in.empty();
    }
}

ipc::Response ServiceCore::handlePrinterPrint(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto printer = deviceManager_.getDefaultPrinter();
    if (!printer) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto err = std::make_shared<ipc::Error>();
        err->code = "DEVICE_NOT_FOUND";
        err->message = "No printer registered";
        resp.error = err;
        return resp;
    }
    auto itJob = cmd.payload.find("jobId");
    if (itJob == cmd.payload.end()) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto err = std::make_shared<ipc::Error>();
        err->code = "INVALID_PAYLOAD";
        err->message = "Missing jobId";
        resp.error = err;
        return resp;
    }
    const std::string jobId = itJob->second;
    auto itPath = cmd.payload.find("filePath");
    if (itPath != cmd.payload.end() && !itPath->second.empty()) {
        // filePath: print from file in background (Bitmap::FromFile; no stream/corrupt JPEG)
        std::string path = itPath->second;
        std::string orientation = "portrait";
        auto itOri = cmd.payload.find("orientation");
        if (itOri != cmd.payload.end() && (itOri->second == "portrait" || itOri->second == "landscape"))
            orientation = itOri->second;
        logging::Logger::getInstance().info("printer_print: file path=" + path + " orientation=" + orientation + " (print in background)");
        bool pathExists = std::filesystem::exists(std::filesystem::path(path));
        logging::Logger::getInstance().info("printer_print: file exists=" + std::string(pathExists ? "yes" : "no"));
        resp.status = ipc::ResponseStatus::OK;
        resp.responseMap["jobId"] = jobId;
        resp.responseMap["deviceId"] = printer->getDeviceInfo().deviceId;
        std::shared_ptr<devices::IPrinter> pr = printer;
        std::thread([pr, jobId, path, orientation]() {
            pr->printFromFile(jobId, path, orientation);
        }).detach();
        return resp;
    }
    auto itData = cmd.payload.find("data");
    if (itData != cmd.payload.end() && !itData->second.empty()) {
        // base64 data: decode then print in background
        std::vector<uint8_t> data;
        if (!base64Decode(itData->second, data)) {
            resp.status = ipc::ResponseStatus::REJECTED;
            auto err = std::make_shared<ipc::Error>();
            err->code = "INVALID_PAYLOAD";
            err->message = "Invalid base64 data";
            resp.error = err;
            return resp;
        }
        resp.status = ipc::ResponseStatus::OK;
        resp.responseMap["jobId"] = jobId;
        resp.responseMap["deviceId"] = printer->getDeviceInfo().deviceId;
        std::shared_ptr<devices::IPrinter> pr = printer;
        std::thread([pr, jobId, data]() {
            pr->print(jobId, data);
        }).detach();
        return resp;
    }
    resp.status = ipc::ResponseStatus::REJECTED;
    auto err = std::make_shared<ipc::Error>();
    err->code = "INVALID_PAYLOAD";
    err->message = "Missing filePath or data";
    resp.error = err;
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
        resp.responseMap["commandId"] = cmd.commandId;
        resp.responseMap["deviceId"] = info.deviceId;
        resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
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
        resp.responseMap["commandId"] = cmd.commandId;
        resp.responseMap["deviceId"] = info.deviceId;
        resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
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
    resp.responseMap["deviceId"] = info.deviceId;
    resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
    resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
    resp.responseMap["deviceName"] = info.deviceName;
    resp.responseMap["lastError"] = info.lastError;
    
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
        resp.responseMap["commandId"] = cmd.commandId;
        resp.responseMap["deviceId"] = info.deviceId;
        resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
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
        resp.responseMap["commandId"] = cmd.commandId;
        resp.responseMap["deviceId"] = info.deviceId;
        resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
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
    resp.responseMap["uid"] = uidHex;
    resp.responseMap["uidLength"] = std::to_string(cardResponse.uid.size());
    
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
    resp.responseMap["data"] = dataHex;
    resp.responseMap["dataLength"] = std::to_string(lastApproval.data.size());
    
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
    resp.responseMap["cardStatus"] = std::string(1, icResponse.cardStatus);
    resp.responseMap["cardInserted"] = (icResponse.cardStatus == 'O') ? "true" : "false";
    
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
    resp.responseMap["screenBrightness"] = std::to_string(settingResponse.screenBrightness);
    resp.responseMap["soundVolume"] = std::to_string(settingResponse.soundVolume);
    resp.responseMap["touchSoundVolume"] = std::to_string(settingResponse.touchSoundVolume);
    
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
    resp.responseMap["transactionType"] = std::string(1, cancelResponse.transactionType);
    resp.responseMap["transactionMedium"] = std::string(1, cancelResponse.transactionMedium);
    resp.responseMap["cardNumber"] = cancelResponse.cardNumber;
    resp.responseMap["approvalAmount"] = cancelResponse.approvalAmount;
    resp.responseMap["approvalNumber"] = cancelResponse.approvalNumber;
    resp.responseMap["salesDate"] = cancelResponse.salesDate;
    resp.responseMap["salesTime"] = cancelResponse.salesTime;
    resp.responseMap["transactionId"] = cancelResponse.transactionId;
    resp.responseMap["isRejected"] = cancelResponse.isRejected() ? "true" : "false";
    resp.responseMap["isSuccess"] = cancelResponse.isSuccess() ? "true" : "false";
    
    return resp;
}

void ServiceCore::prepareEventCallbacks() {
    setupEventCallbacks();
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
    
    // Setup camera event callbacks
    auto camera = deviceManager_.getDefaultCamera();
    if (camera) {
        camera->setCaptureCompleteCallback([this](const devices::CaptureCompleteEvent& event) {
            publishCameraCaptureCompleteEvent(event);
        });
        camera->setStateChangedCallback([this](devices::DeviceState state) {
            publishDeviceStateChangedEvent("camera", state);
        });
        logging::Logger::getInstance().info("Camera capture_complete and state_changed callbacks registered");
    } else {
        logging::Logger::getInstance().warn("setupEventCallbacks: no camera available, capture_complete will not be sent");
    }

    // Setup printer event callback
    auto printer = deviceManager_.getDefaultPrinter();
    if (printer) {
        printer->setPrintJobCompleteCallback([this](const devices::PrintJobCompleteEvent& event) {
            publishPrinterJobCompleteEvent(event);
        });
        printer->setStateChangedCallback([this](devices::DeviceState state) {
            publishDeviceStateChangedEvent("printer", state);
        });
    }
}

void ServiceCore::publishPrinterJobCompleteEvent(const devices::PrintJobCompleteEvent& event) {
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::PRINTER_JOB_COMPLETE;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = "printer";
    ipcEvent.data["jobId"] = event.jobId;
    ipcEvent.data["success"] = event.success ? "true" : "false";
    ipcEvent.data["errorMessage"] = event.errorMessage;
    ipcEvent.data["state"] = std::to_string(static_cast<int>(event.state));
    ipcServer_.broadcastEvent(ipcEvent);
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
    // Full approval detail (for server/store)
    ipcEvent.data["status"] = event.status;
    ipcEvent.data["transactionType"] = event.transactionType;
    ipcEvent.data["approvalAmount"] = event.approvalAmount;
    ipcEvent.data["tax"] = event.tax;
    ipcEvent.data["serviceCharge"] = event.serviceCharge;
    ipcEvent.data["installments"] = event.installments;
    ipcEvent.data["merchantNumber"] = event.merchantNumber;
    ipcEvent.data["terminalNumber"] = event.terminalNumber;
    ipcEvent.data["issuer"] = event.issuer;
    ipcEvent.data["acquirer"] = event.acquirer;
    
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

void ServiceCore::resetOnClientDisconnect() {
    // Cancel any ongoing payment
    auto terminal = deviceManager_.getDefaultPaymentTerminal();
    if (terminal) {
        auto info = terminal->getDeviceInfo();
        if (info.state == devices::DeviceState::STATE_PROCESSING) {
            terminal->cancelPayment();
            logging::Logger::getInstance().info("Payment cancelled due to pipe disconnect");
        }
    }
    // Stop camera liveview so next client gets clean state
    auto camera = deviceManager_.getDefaultCamera();
    if (camera) {
        if (camera->stopPreview()) {
            logging::Logger::getInstance().info("Liveview stopped due to pipe disconnect");
        }
    }
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
            if (info.state == devices::DeviceState::STATE_PROCESSING) {
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
            if (info.state == devices::DeviceState::STATE_ERROR || info.state == devices::DeviceState::DISCONNECTED) {
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
            
            if (info.state == devices::DeviceState::STATE_ERROR || info.state == devices::DeviceState::DISCONNECTED) {
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
            
            if (info.state == devices::DeviceState::STATE_ERROR || info.state == devices::DeviceState::DISCONNECTED) {
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
    for (const auto& pair : deviceStatuses) {
        const std::string& deviceId = pair.first;
        const devices::DeviceInfo& info = pair.second;
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

ipc::Response ServiceCore::handleCameraSetSession(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto it = cmd.payload.find("sessionId");
    if (it == cmd.payload.end()) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PAYLOAD";
        error->message = "Missing 'sessionId' parameter";
        resp.error = error;
        return resp;
    }
    config::ConfigManager::getInstance().setSessionId(it->second);
    resp.status = ipc::ResponseStatus::OK;
    resp.responseMap["sessionId"] = it->second;
    return resp;
}

ipc::Response ServiceCore::handleCameraCapture(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // sessionId required: folder is created/used per capture
    auto itSession = cmd.payload.find("sessionId");
    if (itSession == cmd.payload.end() || itSession->second.empty()) {
        logging::Logger::getInstance().warn("Camera capture failed: Missing 'sessionId' parameter");
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PAYLOAD";
        error->message = "Missing 'sessionId' parameter";
        resp.error = error;
        return resp;
    }
    config::ConfigManager::getInstance().setSessionId(itSession->second);
    
    auto it = cmd.payload.find("captureId");
    if (it == cmd.payload.end()) {
        logging::Logger::getInstance().warn("Camera capture failed: Missing 'captureId' parameter");
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "INVALID_PAYLOAD";
        error->message = "Missing 'captureId' parameter";
        resp.error = error;
        return resp;
    }
    
    // Validate device exists
    auto camera = deviceManager_.getDefaultCamera();
    if (!camera) {
        logging::Logger::getInstance().warn("Camera capture failed: No camera registered");
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No camera registered";
        resp.error = error;
        return resp;
    }
    
    // Execute capture (async)
    std::string captureId = it->second;
    if (camera->capture(captureId)) {
        resp.status = ipc::ResponseStatus::OK;
        auto info = camera->getDeviceInfo();
        resp.responseMap["commandId"] = cmd.commandId;
        resp.responseMap["captureId"] = captureId;
        resp.responseMap["deviceId"] = info.deviceId;
        resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto info = camera->getDeviceInfo();
        auto error = std::make_shared<ipc::Error>();
        error->code = "CAMERA_CAPTURE_FAILED";
        error->message = info.lastError;
        resp.error = error;
    }
    
    return resp;
}

ipc::Response ServiceCore::handleCameraStatus(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto camera = deviceManager_.getDefaultCamera();
    if (!camera) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No camera registered";
        resp.error = error;
        return resp;
    }
    
    auto info = camera->getDeviceInfo();
    resp.responseMap["deviceId"] = info.deviceId;
    resp.responseMap["state"] = std::to_string(static_cast<int>(info.state));
    resp.responseMap["stateString"] = devices::deviceStateToString(info.state);
    resp.responseMap["deviceName"] = info.deviceName;
    resp.responseMap["lastError"] = info.lastError;
    
    return resp;
}

ipc::Response ServiceCore::handleCameraStartPreview(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto camera = deviceManager_.getDefaultCamera();
    if (!camera) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No camera registered";
        resp.error = error;
        return resp;
    }
    
    if (camera->startPreview()) {
        resp.status = ipc::ResponseStatus::OK;
        auto* edsdkCam = dynamic_cast<canon::EdsdkCameraAdapter*>(camera.get());
        if (edsdkCam)
            resp.responseMap["liveview_url"] = edsdkCam->getLiveviewUrl();
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "PREVIEW_START_FAILED";
        error->message = "Failed to start preview";
        resp.error = error;
    }
    return resp;
}

ipc::Response ServiceCore::handleCameraStopPreview(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto camera = deviceManager_.getDefaultCamera();
    if (!camera) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No camera registered";
        resp.error = error;
        return resp;
    }
    
    if (camera->stopPreview()) {
        resp.status = ipc::ResponseStatus::OK;
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "PREVIEW_STOP_FAILED";
        error->message = "Failed to stop preview";
        resp.error = error;
    }
    
    return resp;
}

ipc::Response ServiceCore::handleCameraSetSettings(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto camera = deviceManager_.getDefaultCamera();
    if (!camera) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No camera registered";
        resp.error = error;
        return resp;
    }
    
    devices::CameraSettings settings = camera->getSettings();
    
    // Update settings from payload
    auto widthIt = cmd.payload.find("resolutionWidth");
    auto heightIt = cmd.payload.find("resolutionHeight");
    auto formatIt = cmd.payload.find("imageFormat");
    auto qualityIt = cmd.payload.find("quality");
    auto autoFocusIt = cmd.payload.find("autoFocus");
    
    if (widthIt != cmd.payload.end()) {
        settings.resolutionWidth = std::stoul(widthIt->second);
    }
    if (heightIt != cmd.payload.end()) {
        settings.resolutionHeight = std::stoul(heightIt->second);
    }
    if (formatIt != cmd.payload.end()) {
        settings.imageFormat = formatIt->second;
    }
    if (qualityIt != cmd.payload.end()) {
        settings.quality = std::stoul(qualityIt->second);
    }
    if (autoFocusIt != cmd.payload.end()) {
        settings.autoFocus = (autoFocusIt->second == "true" || autoFocusIt->second == "1");
    }
    
    if (camera->setSettings(settings)) {
        resp.status = ipc::ResponseStatus::OK;
        resp.responseMap["resolutionWidth"] = std::to_string(settings.resolutionWidth);
        resp.responseMap["resolutionHeight"] = std::to_string(settings.resolutionHeight);
        resp.responseMap["imageFormat"] = settings.imageFormat;
        resp.responseMap["quality"] = std::to_string(settings.quality);
        resp.responseMap["autoFocus"] = settings.autoFocus ? "true" : "false";
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "SETTINGS_FAILED";
        error->message = "Failed to set camera settings";
        resp.error = error;
    }
    
    return resp;
}

void ServiceCore::tryReconnectDevicesBeforeDetect() {
    using namespace devices;

    // 자동탐지 시 항상 재연결(프로브)하여 실제 연결 상태 반영. (꺼져 있어도 READY로 캐시된 상태가 갱신됨)

    // 1. Camera — 항상 shutdown + initialize로 실제 연결 여부 확인 (EDSDK만 지원)
    auto camera = deviceManager_.getDefaultCamera();
    if (camera) {
        auto* edsdkCam = dynamic_cast<canon::EdsdkCameraAdapter*>(camera.get());
        if (edsdkCam) {
            logging::Logger::getInstance().info("Detect hardware: probing camera (shutdown + re-init)");
            edsdkCam->shutdown();
            bool ok = edsdkCam->initialize();
            if (ok) {
                logging::Logger::getInstance().info("Detect hardware: camera probe succeeded (READY)");
            } else {
                logging::Logger::getInstance().info("Detect hardware: camera probe failed (disconnected/error), will report current state");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    // 2. Payment — 항상 checkDevice()로 실제 연결 상태 확인
    auto payment = deviceManager_.getDefaultPaymentTerminal();
    if (payment) {
        auto* smartro = dynamic_cast<smartro::SmartroPaymentAdapter*>(payment.get());
        if (smartro) {
            logging::Logger::getInstance().info("Detect hardware: probing payment terminal");
            bool ok = smartro->checkDevice();
            if (ok) {
                logging::Logger::getInstance().info("Detect hardware: payment probe succeeded");
            } else {
                logging::Logger::getInstance().info("Detect hardware: payment probe failed, will report current state");
            }
        }
    }
}

ipc::Response ServiceCore::handleCameraReconnect(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    auto camera = deviceManager_.getDefaultCamera();
    if (!camera) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "DEVICE_NOT_FOUND";
        error->message = "No camera registered";
        resp.error = error;
        return resp;
    }
    
    auto* edsdkCam = dynamic_cast<canon::EdsdkCameraAdapter*>(camera.get());
    if (!edsdkCam) {
        resp.status = ipc::ResponseStatus::REJECTED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "UNSUPPORTED";
        error->message = "Camera reconnect only supported for EDSDK camera";
        resp.error = error;
        return resp;
    }
    
    logging::Logger::getInstance().info("Camera reconnect: shutting down then re-initializing");
    edsdkCam->shutdown();
    bool ok = edsdkCam->initialize();
    if (ok) {
        resp.status = ipc::ResponseStatus::OK;
        resp.responseMap["status"] = "ok";
        logging::Logger::getInstance().info("Camera reconnect completed successfully");
    } else {
        resp.status = ipc::ResponseStatus::FAILED;
        auto error = std::make_shared<ipc::Error>();
        error->code = "RECONNECT_FAILED";
        error->message = "Camera re-initialization failed";
        resp.error = error;
        logging::Logger::getInstance().warn("Camera reconnect: re-initialization failed");
    }
    return resp;
}

void ServiceCore::publishCameraCaptureCompleteEvent(const devices::CaptureCompleteEvent& event) {
    logging::Logger::getInstance().info("=== Publishing CAMERA_CAPTURE_COMPLETE ===");
    logging::Logger::getInstance().info("  filePath: " + event.filePath + ", captureId: " + event.captureId + ", success: " + (event.success ? "true" : "false"));
    ipc::Event ipcEvent;
    ipcEvent.protocolVersion = ipc::PROTOCOL_VERSION;
    ipcEvent.kind = ipc::MessageKind::EVENT;
    ipcEvent.eventId = generateUUID();
    ipcEvent.eventType = ipc::EventType::CAMERA_CAPTURE_COMPLETE;
    ipcEvent.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ipcEvent.deviceType = "camera";

    ipcEvent.data["captureId"] = event.captureId;
    ipcEvent.data["filePath"] = event.filePath;
    ipcEvent.data["imageIndex"] = event.imageIndex;
    ipcEvent.data["success"] = event.success ? "true" : "false";
    ipcEvent.data["imageFormat"] = event.imageFormat;
    ipcEvent.data["width"] = std::to_string(event.width);
    ipcEvent.data["height"] = std::to_string(event.height);
    ipcEvent.data["state"] = std::to_string(static_cast<int>(event.state));
    if (!event.success) {
        ipcEvent.data["errorMessage"] = event.errorMessage;
    }
    ipcServer_.broadcastEvent(ipcEvent);
}

} // namespace core
