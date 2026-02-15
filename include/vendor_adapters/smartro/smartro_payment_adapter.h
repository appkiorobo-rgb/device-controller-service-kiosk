// include/vendor_adapters/smartro/smartro_payment_adapter.h
#pragma once

#include "devices/ipayment_terminal.h"
#include "vendor_adapters/smartro/smartro_comm.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include "vendor_adapters/smartro/serial_port.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

namespace smartro {

// Smartro payment terminal adapter (implements IPaymentTerminal)
class SmartroPaymentAdapter : public devices::IPaymentTerminal {
public:
    SmartroPaymentAdapter(const std::string& deviceId, 
                         const std::string& comPort,
                         const std::string& terminalId);
    ~SmartroPaymentAdapter();
    
    // IPaymentTerminal implementation (core)
    devices::DeviceInfo getDeviceInfo() const override;
    bool startPayment(uint32_t amount) override;
    bool cancelPayment() override;
    devices::DeviceState getState() const override;
    bool reset() override;
    bool checkDevice() override;
    std::string getVendorName() const override { return "smartro"; }
    std::string getComPort() const override { return comPort_; }
    bool reconnect(const std::string& newPort) override;

    // IPaymentTerminal extended operations (vendor-agnostic interface)
    devices::CardUidResult readCardUid() override;
    devices::IcCardCheckResult checkIcCard() override;
    bool setScreenSound(const devices::ScreenSoundSettings& request, devices::ScreenSoundSettings& response) override;
    devices::TransactionCancelResult cancelTransaction(const devices::TransactionCancelRequest& request) override;
    devices::PaymentCompleteEvent getLastApproval(const std::string& transactionType) override;

    // Smartro-specific methods (use vendor types directly; prefer interface methods above for new code)
    bool readCardUidRaw(smartro::CardUidReadResponse& response);
    bool getLastApprovalRaw(smartro::LastApprovalResponse& response);
    bool checkIcCardRaw(smartro::IcCardCheckResponse& response);
    bool setScreenSoundRaw(const smartro::ScreenSoundSettingRequest& request, smartro::ScreenSoundSettingResponse& response);
    bool cancelTransactionRaw(const smartro::TransactionCancelRequest& request, smartro::TransactionCancelResponse& response);

    /// Static port probe for auto-detect: returns true if a Smartro terminal responds on the given port.
    static bool tryPort(const std::string& port);
    
    void setPaymentCompleteCallback(std::function<void(const devices::PaymentCompleteEvent&)> callback) override;
    void setPaymentFailedCallback(std::function<void(const devices::PaymentFailedEvent&)> callback) override;
    void setPaymentCancelledCallback(std::function<void(const devices::PaymentCancelledEvent&)> callback) override;
    void setStateChangedCallback(std::function<void(devices::DeviceState)> callback) override;
    
private:
    void updateState(devices::DeviceState newState);
    void processPaymentResponse(const PaymentApprovalResponse& response);
    void processEvent(const EventResponse& event);
    void eventMonitorThread();
    
    std::string deviceId_;
    std::string comPort_;
    std::string terminalId_;
    
    std::unique_ptr<SerialPort> serialPort_;
    std::unique_ptr<SmartroComm> smartroComm_;
    
    mutable std::mutex stateMutex_;
    devices::DeviceState state_;
    std::string lastError_;
    
    // Payment progress state
    std::atomic<bool> paymentInProgress_;
    std::atomic<bool> paymentCancelled_;  // Flag to indicate payment was cancelled
    uint32_t currentAmount_;
    
    // Event callbacks
    std::function<void(const devices::PaymentCompleteEvent&)> paymentCompleteCallback_;
    std::function<void(const devices::PaymentFailedEvent&)> paymentFailedCallback_;
    std::function<void(const devices::PaymentCancelledEvent&)> paymentCancelledCallback_;
    std::function<void(devices::DeviceState)> stateChangedCallback_;
    
    // Event monitoring thread
    std::atomic<bool> monitorRunning_;
    std::thread monitorThread_;
    
    std::chrono::system_clock::time_point lastUpdateTime_;
};

} // namespace smartro
