// src/core/service_core_detect_hardware.cpp
// handleDetectHardware 전용 — 결제 단말기(카드)와 현금결제기(LV77) 완전 분리.
// Vendor-agnostic: 인터페이스만 사용, dynamic_cast 없음.
#include "core/service_core.h"
#include "core/device_constants.h"
#include "config/config_manager.h"
#include "devices/device_types.h"
#include "devices/icamera.h"
#include "devices/iprinter.h"
#include "devices/ipayment_terminal.h"
#include "devices/payment_terminal_factory.h"
#include "ipc/message_types.h"
#include "vendor_adapters/smartro/serial_port.h"
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

    // config.ini를 다시 읽어 cash.enabled=0 등 최신 설정 반영 (수동 편집·다른 저장 대비)
    config::ConfigManager::getInstance().reloadFromFileIfExists();
    auto config = config::ConfigManager::getInstance().getAll();
    bool paymentEnabled = isEnabled(cmd.payload, config, "payment.enabled");
    bool cashEnabled    = isEnabled(cmd.payload, config, "cash.enabled");

    // probe=false: 현재 상태만 수집 (checkDevice/COM 스캔 생략 → 빠름)
    auto it = cmd.payload.find("probe");
    bool doProbe = (it == cmd.payload.end() || it->second != "false");
    std::vector<std::string> availablePorts;
    if (doProbe)
        availablePorts = smartro::SerialPort::getAvailablePorts(true);

    // 1. Camera
    auto camera = deviceManager_.getDefaultCamera();
    if (camera) {
        auto info = camera->getDeviceInfo();
        resp.responseMap["camera.model"] = info.deviceName;
        resp.responseMap["camera.state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["camera.stateString"] = devices::deviceStateToString(info.state);
        resp.responseMap["camera.lastError"] = info.lastError;
    }

    // 2. Printer
    auto printer = deviceManager_.getDefaultPrinter();
    if (printer) {
        auto info = printer->getDeviceInfo();
        resp.responseMap["printer.name"] = info.deviceName;
        resp.responseMap["printer.state"] = std::to_string(static_cast<int>(info.state));
        resp.responseMap["printer.stateString"] = devices::deviceStateToString(info.state);
        resp.responseMap["printer.lastError"] = info.lastError;
        logging::Logger::getInstance().debug("Detect hardware: printer \"" + info.deviceName + "\" state=" + devices::deviceStateToString(info.state));
    }

    // 3. Payment (카드 결제 단말기 — LV77와 완전 분리)
    //    tryReconnectDevicesBeforeDetect()가 이미 checkDevice()를 호출했으므로 여기서는 상태만 수집.
    //    만약 tryReconnect에서 팩토리 자동감지로 새로 등록했다면 여기서 상태를 수집할 수 있음.
    if (paymentEnabled) {
        auto paymentTerminal = deviceManager_.getPaymentTerminal(kCardTerminalId);

        // tryReconnect에서 등록 못 했거나 probe=false(경량)이면 여기서 팩토리 시도
        if (!paymentTerminal && doProbe && !availablePorts.empty()) {
            std::string cashCom;
            if (config.count("cash.com_port")) cashCom = config["cash.com_port"];
            logging::Logger::getInstance().info("Detect hardware: payment terminal not registered, trying factory auto-detect");
            auto [vendor, adapter] = devices::PaymentTerminalFactory::detectOnPorts(
                kCardTerminalId, availablePorts, cashCom, "card");
            if (adapter) {
                logging::Logger::getInstance().info(
                    "Detect hardware: factory detected payment terminal (" + vendor + ") on " + adapter->getComPort());
                deviceManager_.registerPaymentTerminal(kCardTerminalId, adapter);
                paymentTerminal = adapter;
            }
        }

        if (paymentTerminal) {
            // Collect COM port (already probed by tryReconnect or factory; no re-probe here)
            std::string port = paymentTerminal->getComPort();
            if (!port.empty())
                resp.responseMap["payment.com_port"] = port;

            auto info = paymentTerminal->getDeviceInfo();
            resp.responseMap["payment.state"] = std::to_string(static_cast<int>(info.state));
            resp.responseMap["payment.stateString"] = devices::deviceStateToString(info.state);
            resp.responseMap["payment.lastError"] = info.lastError;
            resp.responseMap["payment.vendor"] = paymentTerminal->getVendorName();
        }
        // Fallback to config port if not detected
        if (resp.responseMap.find("payment.com_port") == resp.responseMap.end() && config.count("payment.com_port"))
            resp.responseMap["payment.com_port"] = config["payment.com_port"];
    }

    // 4. Cash (현금결제기) — cash.enabled일 때만 검사·상태 반환. 비사용 시 통신/접근 없음.
    if (cashEnabled) {
        std::string paymentCom;
        if (resp.responseMap.count("payment.com_port")) paymentCom = resp.responseMap["payment.com_port"];
        else if (config.count("payment.com_port")) paymentCom = config["payment.com_port"];

        if (doProbe && !availablePorts.empty()) {
            // Use factory to auto-detect cash device on available ports (exclude payment port)
            auto [vendor, adapter] = devices::PaymentTerminalFactory::detectOnPorts(
                kCashDeviceId, availablePorts, paymentCom, "cash");
            if (adapter) {
                resp.responseMap["cash.com_port"] = adapter->getComPort();
                resp.responseMap["cash.vendor"] = vendor;
                logging::Logger::getInstance().info(
                    "Detect hardware: cash device (" + vendor + ") found on " + adapter->getComPort() + " (payment on " + paymentCom + ")");
            }
        }

        // Fallback: existing registered terminal or config
        if (resp.responseMap.find("cash.com_port") == resp.responseMap.end()) {
            auto cashTerminal = deviceManager_.getPaymentTerminal(kCashDeviceId);
            if (cashTerminal && !cashTerminal->getComPort().empty())
                resp.responseMap["cash.com_port"] = cashTerminal->getComPort();
            else if (config.count("cash.com_port"))
                resp.responseMap["cash.com_port"] = config["cash.com_port"];
        }

        auto cashTerminal = deviceManager_.getPaymentTerminal(kCashDeviceId);
        if (cashTerminal) {
            auto info = cashTerminal->getDeviceInfo();
            resp.responseMap["cash.state"] = std::to_string(static_cast<int>(info.state));
            resp.responseMap["cash.stateString"] = devices::deviceStateToString(info.state);
            resp.responseMap["cash.lastError"] = info.lastError;
            resp.responseMap["cash.vendor"] = cashTerminal->getVendorName();
        }
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
        // 경량 모드: 설정에 있는 포트만 나열 (payment 항상, cash는 cash.enabled일 때만)
        std::ostringstream oss;
        if (config.count("payment.com_port")) {
            oss << config.at("payment.com_port");
        }
        if (cashEnabled && config.count("cash.com_port")) {
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
