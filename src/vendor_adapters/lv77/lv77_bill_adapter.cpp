// src/vendor_adapters/lv77/lv77_bill_adapter.cpp
#include "logging/logger.h"
#include "vendor_adapters/lv77/lv77_bill_adapter.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>

namespace lv77 {

static std::string makeTransactionId() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream os;
    os << "CASH-" << std::put_time(std::localtime(&t), "%Y%m%d%H%M%S");
    return os.str();
}

Lv77BillAdapter::Lv77BillAdapter(const std::string& deviceId, const std::string& comPort)
    : deviceId_(deviceId)
    , comPort_(comPort)
    , state_(devices::DeviceState::DISCONNECTED)
    , paymentInProgress_(false)
    , paymentCancelled_(false)
    , lastUpdateTime_(std::chrono::system_clock::now()) {
    serialPort_ = std::make_unique<smartro::SerialPort>();
    comm_ = std::make_unique<Lv77Comm>(*serialPort_);
}

Lv77BillAdapter::~Lv77BillAdapter() {
    comm_->stopPollLoop();
    comm_->close();
}

void Lv77BillAdapter::updateState(devices::DeviceState newState) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (state_ != newState) {
        state_ = newState;
        lastUpdateTime_ = std::chrono::system_clock::now();
        if (stateChangedCallback_) stateChangedCallback_(state_);
    }
}

devices::DeviceInfo Lv77BillAdapter::getDeviceInfo() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    devices::DeviceInfo info;
    info.deviceId = deviceId_;
    info.deviceType = devices::DeviceType::PAYMENT_TERMINAL;
    info.deviceName = "LV77 Bill Validator (ICT-104U)";
    info.state = state_;
    info.lastError = lastError_;
    info.lastUpdateTime = lastUpdateTime_;
    return info;
}

bool Lv77BillAdapter::startPayment(uint32_t amount) {
    if (!comm_->isOpen()) {
        if (!comm_->open(comPort_)) {
            lastError_ = "Failed to open " + comPort_;
            logging::Logger::getInstance().warn("[LV77] startPayment: " + lastError_);
            return false;
        }
        if (!comm_->syncAfterPowerUp(2000)) {
            comm_->close();
            lastError_ = "Sync failed";
            return false;
        }
    }
    if (paymentInProgress_) {
        lastError_ = "Payment already in progress";
        return false;
    }
    paymentCancelled_ = false;
    paymentInProgress_ = true;
    targetAmount_ = amount;
    currentTotal_ = 0;
    updateState(devices::DeviceState::STATE_PROCESSING);
    comm_->setBillStackedCallback([this](uint32_t amt) { onBillStacked(amt); });
    // 잔돈 없음: 남은 금액보다 큰 지폐 들어오면 반환 + Flutter에 전달
    comm_->setEscrowCallback([this](uint32_t billAmount) {
        uint32_t target = targetAmount_.load();
        uint32_t current = currentTotal_.load();
        if (target == 0) return true;  // 테스트 모드(0원 결제): 전 수락
        if (current + billAmount <= target) return true;
        devices::PaymentFailedEvent ev;
        ev.errorCode = "CASH_BILL_RETURNED";
        ev.errorMessage = "Exceed target amount (no change); bill returned";
        ev.amount = billAmount;
        ev.state = devices::DeviceState::STATE_PROCESSING;
        if (paymentFailedCallback_) paymentFailedCallback_(ev);
        logging::Logger::getInstance().info("[LV77] Bill returned (exceed target): " + std::to_string(billAmount) + " KRW, target=" + std::to_string(target) + " current=" + std::to_string(current));
        return false;
    });
    if (!comm_->enable()) {
        lastError_ = comm_->getLastError();
        paymentInProgress_ = false;
        updateState(devices::DeviceState::STATE_ERROR);
        return false;
    }
    comm_->startPollLoop(100);
    logging::Logger::getInstance().info("[LV77] Payment started (accepting bills)");
    return true;
}

bool Lv77BillAdapter::cancelPayment() {
    if (!paymentInProgress_) {
        lastError_ = "No payment in progress";
        return true;
    }
    paymentCancelled_ = true;
    paymentInProgress_ = false;
    comm_->stopPollLoop();
    comm_->disable();
    updateState(devices::DeviceState::STATE_READY);
    devices::PaymentCancelledEvent ev;
    ev.state = devices::DeviceState::STATE_READY;
    if (paymentCancelledCallback_) paymentCancelledCallback_(ev);
    logging::Logger::getInstance().info("[LV77] Payment cancelled");
    return true;
}

devices::DeviceState Lv77BillAdapter::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

bool Lv77BillAdapter::reset() {
    if (!comm_->isOpen()) {
        lastError_ = "Device not connected";
        return false;
    }
    comm_->stopPollLoop();
    if (!comm_->reset(3000)) {
        lastError_ = comm_->getLastError();
        return false;
    }
    paymentInProgress_ = false;
    paymentCancelled_ = false;
    updateState(devices::DeviceState::STATE_READY);
    return true;
}

bool Lv77BillAdapter::reconnect(const std::string& newPort) {
    if (newPort.empty()) return false;
    if (paymentInProgress_) {
        cancelPayment();
    }
    comm_->stopPollLoop();
    comm_->close();
    comPort_ = newPort;
    updateState(devices::DeviceState::DISCONNECTED);
    logging::Logger::getInstance().info("[LV77] Reconnected to " + newPort + " (next startPayment will use this port)");
    return true;
}

bool Lv77BillAdapter::checkDevice() {
    lastError_.clear();
    if (comm_->isOpen()) comm_->close();
    std::vector<std::string> ports = smartro::SerialPort::getAvailablePorts();
    if (ports.empty()) {
        lastError_ = "No COM ports available";
        return false;
    }
    for (const auto& port : ports) {
        if (comm_->open(port)) {
            if (comm_->syncAfterPowerUp(2000)) {
                comm_->enable();
                uint8_t status = 0;
                if (comm_->poll(status, 500)) {
                    if (status == STATUS_ENABLE || status == STATUS_INHIBIT) {
                        comPort_ = port;
                        updateState(devices::DeviceState::STATE_READY);
                        logging::Logger::getInstance().info("[LV77] checkDevice OK on " + port);
                        return true;
                    }
                }
            }
            comm_->close();
        }
    }
    lastError_ = "LV77 not found on any COM port";
    updateState(devices::DeviceState::DISCONNECTED);
    return false;
}

bool Lv77BillAdapter::tryPort(const std::string& port) {
    if (port.empty()) return false;
    smartro::SerialPort sp;
    Lv77Comm comm(sp);
    if (!comm.open(port)) return false;
    bool ok = false;
    if (comm.syncAfterPowerUp(2000)) {
        comm.enable();
        uint8_t status = 0;
        if (comm.poll(status, 500))
            ok = (status == STATUS_ENABLE || status == STATUS_INHIBIT);
    }
    comm.close();
    return ok;
}

void Lv77BillAdapter::setPaymentCompleteCallback(std::function<void(const devices::PaymentCompleteEvent&)> callback) {
    paymentCompleteCallback_ = std::move(callback);
}

void Lv77BillAdapter::setPaymentFailedCallback(std::function<void(const devices::PaymentFailedEvent&)> callback) {
    paymentFailedCallback_ = std::move(callback);
}

void Lv77BillAdapter::setPaymentCancelledCallback(std::function<void(const devices::PaymentCancelledEvent&)> callback) {
    paymentCancelledCallback_ = std::move(callback);
}

void Lv77BillAdapter::setStateChangedCallback(std::function<void(devices::DeviceState)> callback) {
    stateChangedCallback_ = std::move(callback);
}

void Lv77BillAdapter::onBillStacked(uint32_t amount) {
    if (paymentCancelled_ || !paymentInProgress_) return;
    currentTotal_ += amount;
    uint32_t currentTotal = currentTotal_.load();
    if (cashBillStackedCallback_) {
        cashBillStackedCallback_(amount, currentTotal);
    } else if (paymentCompleteCallback_) {
        devices::PaymentCompleteEvent ev;
        ev.transactionId = makeTransactionId();
        ev.amount = amount;
        ev.cardNumber = "";
        ev.approvalNumber = "";
        ev.salesDate = "";
        ev.salesTime = "";
        ev.transactionMedium = "CASH";
        ev.state = devices::DeviceState::STATE_READY;
        ev.status = "SUCCESS";
        ev.transactionType = "Cash";
        ev.approvalAmount = std::to_string(amount);
        ev.tax = "";
        ev.serviceCharge = "";
        ev.installments = "";
        ev.merchantNumber = "";
        ev.terminalNumber = "";
        ev.issuer = "";
        ev.acquirer = "";
        paymentCompleteCallback_(ev);
    }
    logging::Logger::getInstance().info("[LV77] Bill accepted: " + std::to_string(amount) + " KRW (total " + std::to_string(currentTotal) + ")");

    // 목표 금액 도달: 폴 스레드에서는 stopPollLoop 호출 금지(자기 join → deadlock/abort). 디테치 스레드에서 처리.
    uint32_t target = targetAmount_.load();
    if (target > 0 && currentTotal_.load() >= target) {
        paymentInProgress_ = false;
        updateState(devices::DeviceState::STATE_READY);
        uint32_t total = currentTotal_.load();
        logging::Logger::getInstance().info("[LV77] Target reached: " + std::to_string(total) + " KRW, deferring stopPollLoop/disable to worker thread");
        std::thread([this, total]() {
            comm_->stopPollLoop();
            comm_->disable();  // 0x5E → 현금결제기 DISABLE
            if (paymentTargetReachedCallback_) paymentTargetReachedCallback_(total);
            logging::Logger::getInstance().info("[LV77] DISABLE (0x5E) sent, cash_payment_target_reached event sent");
        }).detach();
    }
}

void Lv77BillAdapter::setPaymentTargetReachedCallback(std::function<void(uint32_t totalAmount)> callback) {
    paymentTargetReachedCallback_ = std::move(callback);
}

void Lv77BillAdapter::setCashBillStackedCallback(std::function<void(uint32_t amount, uint32_t currentTotal)> callback) {
    cashBillStackedCallback_ = std::move(callback);
}

} // namespace lv77
