// src/core/service_core_detect_hardware.cpp
// handleDetectHardware 전용 - EDSDK/Windows 헤더 제외로 매크로 충돌 방지
#include "core/service_core.h"
#include "config/config_manager.h"
#include "devices/device_types.h"
#include "devices/icamera.h"
#include "devices/iprinter.h"
#include "devices/ipayment_terminal.h"
#include "ipc/message_types.h"
#include "vendor_adapters/smartro/smartro_payment_adapter.h"
#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/lv77/lv77_bill_adapter.h"
#include "logging/logger.h"

#include <sstream>
#include <chrono>
#include <map>
#include <vector>

namespace core {

ipc::Response ServiceCore::handleDetectHardware(const ipc::Command& cmd) {
    ipc::Response resp;
    resp.protocolVersion = cmd.protocolVersion;
    resp.kind = ipc::MessageKind::RESPONSE;
    resp.commandId = cmd.commandId;
    resp.status = ipc::ResponseStatus::OK;
    resp.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    auto config = config::ConfigManager::getInstance().getAll();

    // probe=false: 현재 상태만 수집 (checkDevice/COM 스캔 생략 → 빠름)
    auto it = cmd.payload.find("probe");
    bool doProbe = (it == cmd.payload.end() || it->second != "false");
    std::vector<std::string> availablePorts;
    if (doProbe)
        availablePorts = smartro::SerialPort::getAvailablePorts(true);

    // 1. Camera — 모델명 + 상태(메모리 상 현재 값)
    auto camera = deviceManager_.getDefaultCamera();
    if (camera) {
        auto info = camera->getDeviceInfo();
        resp.responseMap["camera.model"] = info.deviceName;
        resp.responseMap["camera.state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["camera.stateString"] = devices::deviceStateToString(info.state);
        resp.responseMap["camera.lastError"] = info.lastError;
    }

    // 2. Printer — name + state (checked via getState(); no COM port, printer is name-based)
    auto printer = deviceManager_.getDefaultPrinter();
    if (printer) {
        auto info = printer->getDeviceInfo();
        resp.responseMap["printer.name"] = info.deviceName;
        resp.responseMap["printer.state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["printer.stateString"] = devices::deviceStateToString(info.state);
        resp.responseMap["printer.lastError"] = info.lastError;
        logging::Logger::getInstance().debug("Detect hardware: printer \"" + info.deviceName + "\" state=" + devices::deviceStateToString(info.state));
    }

    // 3. Payment — probe=true일 때만 checkDevice() (시리얼 통신·타임아웃 발생)
    auto paymentTerminal = deviceManager_.getDefaultPaymentTerminal();
    if (paymentTerminal) {
        auto smartro = std::dynamic_pointer_cast<smartro::SmartroPaymentAdapter>(paymentTerminal);
        if (smartro) {
            if (doProbe) {
                if (smartro->checkDevice()) {
                    std::string port = smartro->getComPort();
                    if (!port.empty())
                        resp.responseMap["payment.com_port"] = port;
                }
            }
            if (resp.responseMap.find("payment.com_port") == resp.responseMap.end()) {
                std::string port = smartro->getComPort();
                if (!port.empty()) resp.responseMap["payment.com_port"] = port;
            }
        }
        auto info = paymentTerminal->getDeviceInfo();
        resp.responseMap["payment.state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["payment.stateString"] = devices::deviceStateToString(info.state);
        resp.responseMap["payment.lastError"] = info.lastError;
    }
    if (resp.responseMap.find("payment.com_port") == resp.responseMap.end() && config.count("payment.com_port"))
        resp.responseMap["payment.com_port"] = config["payment.com_port"];

    // 4. Cash — probe 시 payment와 다른 COM에서 LV77 탐지, 사용자가 COM 번호를 몰라도 자동으로 카드/현금 구분
    std::string paymentCom;
    if (resp.responseMap.count("payment.com_port")) paymentCom = resp.responseMap["payment.com_port"];
    else if (config.count("payment.com_port")) paymentCom = config["payment.com_port"];
    if (doProbe && !availablePorts.empty()) {
        for (const auto& port : availablePorts) {
            if (!paymentCom.empty() && port == paymentCom) continue;
            if (lv77::Lv77BillAdapter::tryPort(port)) {
                resp.responseMap["cash.com_port"] = port;
                logging::Logger::getInstance().info("Detect hardware: LV77 (cash) found on " + port + " (payment on " + paymentCom + ")");
                break;
            }
        }
    }
    if (resp.responseMap.find("cash.com_port") == resp.responseMap.end()) {
        auto cashTerminal = deviceManager_.getPaymentTerminal("lv77_cash_001");
        auto lv77 = std::dynamic_pointer_cast<lv77::Lv77BillAdapter>(cashTerminal);
        if (lv77 && !lv77->getComPort().empty())
            resp.responseMap["cash.com_port"] = lv77->getComPort();
        else if (config.count("cash.com_port"))
            resp.responseMap["cash.com_port"] = config["cash.com_port"];
    }

    // 5. Available COM ports — probe=true일 때 위에서 구한 목록 사용
    if (doProbe) {
        std::ostringstream oss;
        for (size_t i = 0; i < availablePorts.size(); ++i) {
            if (i > 0) oss << ",";
            oss << availablePorts[i];
        }
        resp.responseMap["available_ports"] = oss.str();
    } else {
        // 경량 모드: 설정에 있는 포트만 나열
        std::ostringstream oss;
        if (config.count("payment.com_port")) {
            oss << config.at("payment.com_port");
        }
        if (config.count("cash.com_port")) {
            std::string cash = config.at("cash.com_port");
            if (oss.str().find(cash) == std::string::npos) {
                if (!oss.str().empty()) oss << ",";
                oss << cash;
            }
        }
        resp.responseMap["available_ports"] = oss.str();
    }

    return resp;
}

} // namespace core
