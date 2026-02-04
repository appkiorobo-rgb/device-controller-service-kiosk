// src/vendor_adapters/lv77/lv77_bill_adapter.cpp
#include "logging/logger.h"
#include "vendor_adapters/lv77/lv77_bill_adapter.h"
#include <chrono>
#include <sstream>
#include <iomanip>

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

bool Lv77BillAdapter::startPayment(uint32_t /*amount*/) {
    if (!comm_->isOpen()) {
        lastError_ = "Device not connected";
        logging::Logger::getInstance().warn("[LV77] startPayment: " + lastError_);
        return false;
    }
    if (paymentInProgress_) {
        lastError_ = "Payment already in progress";
        return false;
    }
    paymentCancelled_ = false;
    paymentInProgress_ = true;
    updateState(devices::DeviceState::STATE_PROCESSING);
    comm_->setBillStackedCallback([this](uint32_t amount) { onBillStacked(amount); });
    if (!comm_->enable()) {
        lastError_ = comm_->getLastError();
        paymentInProgress_ = false;
        updateState(devices::DeviceState::STATE_ERROR);
        return false;
    }
    comm_->startPollLoop(500);
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
    if (paymentCompleteCallback_) paymentCompleteCallback_(ev);
    logging::Logger::getInstance().info("[LV77] Bill accepted: " + std::to_string(amount) + " KRW");
}

} // namespace lv77
