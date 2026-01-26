// include/vendor_adapters/smartro/smartro_payment_terminal.h
#pragma once

#include "device_abstraction/ipayment_terminal.h"
#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace device_controller::vendor::smartro {

// SMARTROPaymentTerminal - SMARTRO payment terminal adapter
// Implements IPaymentTerminal interface
class SMARTROPaymentTerminal : public device_controller::IPaymentTerminal {
public:
    SMARTROPaymentTerminal();
    ~SMARTROPaymentTerminal();

    // IPaymentTerminal interface
    PaymentTerminalState getState() const override;
    bool initialize() override;
    void shutdown() override;
    bool startPayment(int64_t amount) override;
    void cancelPayment() override;
    void setEventCallback(PaymentEventCallback callback) override;
    std::string getDeviceId() const override;
    std::string getDeviceName() const override;

    // Additional methods for SMARTRO-specific features
    bool checkDeviceStatus();
    bool resetDevice();

private:
    mutable std::mutex mutex_;
    PaymentTerminalState state_;
    PaymentEventCallback eventCallback_;
    
    std::shared_ptr<SerialPort> serialPort_;
    std::shared_ptr<SMARTROProtocol> protocol_;
    
    std::string portName_;
    std::string deviceId_;
    std::string deviceName_;
    
    std::atomic<bool> initialized_{false};
    std::atomic<bool> paymentInProgress_{false};
    
    int64_t currentPaymentAmount_;
    std::chrono::steady_clock::time_point lastActivityTime_;
    
    // State management
    void setState(PaymentTerminalState newState);
    void notifyEvent(PaymentEventType type, const std::string& errorCode = "", 
                    const std::string& errorMessage = "");
    
    // Protocol event handler
    void onProtocolEvent(char eventCode, const std::string& data);
    
    // Connection management
    bool detectAndConnect();
    void monitorConnection();
    std::thread monitorThread_;
    std::atomic<bool> monitoring_{false};
    
    // Recovery
    void attemptReconnect();
    bool checkHungState();
    static constexpr int HUNG_TIMEOUT_MS = 30000; // 30 seconds
};

} // namespace device_controller::vendor::smartro
