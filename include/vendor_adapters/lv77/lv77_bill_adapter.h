// include/vendor_adapters/lv77/lv77_bill_adapter.h
#pragma once

#include "devices/ipayment_terminal.h"
#include "vendor_adapters/lv77/lv77_comm.h"
#include "vendor_adapters/smartro/serial_port.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace lv77 {

// LV77 bill validator adapter (ICT-104U); implements IPaymentTerminal for cash payment
class Lv77BillAdapter : public devices::IPaymentTerminal {
public:
    Lv77BillAdapter(const std::string& deviceId, const std::string& comPort);
    ~Lv77BillAdapter();

    devices::DeviceInfo getDeviceInfo() const override;
    bool startPayment(uint32_t amount) override;
    bool cancelPayment() override;
    devices::DeviceState getState() const override;
    bool reset() override;
    bool checkDevice() override;

    std::string getComPort() const { return comPort_; }

    void setPaymentCompleteCallback(std::function<void(const devices::PaymentCompleteEvent&)> callback) override;
    void setPaymentFailedCallback(std::function<void(const devices::PaymentFailedEvent&)> callback) override;
    void setPaymentCancelledCallback(std::function<void(const devices::PaymentCancelledEvent&)> callback) override;
    void setStateChangedCallback(std::function<void(devices::DeviceState)> callback) override;

private:
    void updateState(devices::DeviceState newState);
    void onBillStacked(uint32_t amount);

    std::string deviceId_;
    std::string comPort_;
    std::unique_ptr<smartro::SerialPort> serialPort_;
    std::unique_ptr<Lv77Comm> comm_;

    mutable std::mutex stateMutex_;
    devices::DeviceState state_;
    std::string lastError_;
    std::atomic<bool> paymentInProgress_;
    std::atomic<bool> paymentCancelled_;

    std::function<void(const devices::PaymentCompleteEvent&)> paymentCompleteCallback_;
    std::function<void(const devices::PaymentFailedEvent&)> paymentFailedCallback_;
    std::function<void(const devices::PaymentCancelledEvent&)> paymentCancelledCallback_;
    std::function<void(devices::DeviceState)> stateChangedCallback_;
    std::chrono::system_clock::time_point lastUpdateTime_;
};

} // namespace lv77
