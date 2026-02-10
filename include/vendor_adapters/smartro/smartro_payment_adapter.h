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
    
    // IPaymentTerminal implementation
    devices::DeviceInfo getDeviceInfo() const override;
    bool startPayment(uint32_t amount) override;
    bool cancelPayment() override;
    devices::DeviceState getState() const override;
    bool reset() override;
    bool checkDevice() override;
    
    // Get COM port
    std::string getComPort() const { return comPort_; }

    /** Reconnect to a different COM port (e.g. after Admin set_config). Closes current port and runs device check on new port. */
    bool reconnect(const std::string& port);

    // Additional Smartro-specific methods
    bool readCardUid(smartro::CardUidReadResponse& response);
    bool getLastApproval(smartro::LastApprovalResponse& response);
    bool checkIcCard(smartro::IcCardCheckResponse& response);
    bool setScreenSound(const smartro::ScreenSoundSettingRequest& request, smartro::ScreenSoundSettingResponse& response);
    bool cancelTransaction(const smartro::TransactionCancelRequest& request, smartro::TransactionCancelResponse& response);
    
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
