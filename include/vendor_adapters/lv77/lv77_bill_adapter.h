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

    /// Single-port probe for auto-detect: returns true if LV77 responds on the given port (opens/closes internally).
    static bool tryPort(const std::string& port);

    /// Close current port and set new COM for next startPayment (set_config 시 현금 COM 변경 반영).
    bool reconnect(const std::string& port);

    std::string getComPort() const { return comPort_; }

    void setPaymentCompleteCallback(std::function<void(const devices::PaymentCompleteEvent&)> callback) override;
    void setPaymentFailedCallback(std::function<void(const devices::PaymentFailedEvent&)> callback) override;
    void setPaymentCancelledCallback(std::function<void(const devices::PaymentCancelledEvent&)> callback) override;
    void setStateChangedCallback(std::function<void(devices::DeviceState)> callback) override;

    /// 목표 금액 도달 시 호출 (현금 세션 완료). LV77 전용; 설정 시 금액 도달 후 0x5E(DISABLE) 전송 후 콜백 호출.
    void setPaymentTargetReachedCallback(std::function<void(uint32_t totalAmount)> callback);

    /// 지폐 수락 시 호출 (amount, currentTotal). cash_bill_stacked 이벤트용. 설정 시 paymentCompleteCallback_ 대신 사용.
    void setCashBillStackedCallback(std::function<void(uint32_t amount, uint32_t currentTotal)> callback);

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
    /// 결제 목표 금액 (startPayment(amount) 시 설정). 0이면 테스트 모드(전 수락).
    std::atomic<uint32_t> targetAmount_{0};
    /// 현재까지 수락한 누적 금액 (잔돈 없음 → 초과분 반환)
    std::atomic<uint32_t> currentTotal_{0};

    std::function<void(const devices::PaymentCompleteEvent&)> paymentCompleteCallback_;
    std::function<void(const devices::PaymentFailedEvent&)> paymentFailedCallback_;
    std::function<void(const devices::PaymentCancelledEvent&)> paymentCancelledCallback_;
    std::function<void(uint32_t totalAmount)> paymentTargetReachedCallback_;
    std::function<void(uint32_t amount, uint32_t currentTotal)> cashBillStackedCallback_;
    std::function<void(devices::DeviceState)> stateChangedCallback_;
    std::chrono::system_clock::time_point lastUpdateTime_;
};

} // namespace lv77
