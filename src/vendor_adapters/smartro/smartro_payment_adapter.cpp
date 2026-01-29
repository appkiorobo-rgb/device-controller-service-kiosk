// src/vendor_adapters/smartro/smartro_payment_adapter.cpp
// logger.h???????? include??? Windows SDK ?? ???
#include "logging/logger.h"
#include "vendor_adapters/smartro/smartro_payment_adapter.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <sstream>

namespace smartro {

SmartroPaymentAdapter::SmartroPaymentAdapter(const std::string& deviceId,
                                             const std::string& comPort,
                                             const std::string& terminalId)
    : deviceId_(deviceId)
    , comPort_(comPort)
    , terminalId_(terminalId)
    , state_(devices::DeviceState::DISCONNECTED)
    , paymentInProgress_(false)
    , paymentCancelled_(false)
    , currentAmount_(0)
    , monitorRunning_(false) {
    
    serialPort_ = std::make_unique<SerialPort>();
    smartroComm_ = std::make_unique<SmartroComm>(*serialPort_);
    
    lastUpdateTime_ = std::chrono::system_clock::now();
    
    // Try initial connection
    checkDevice();
}

SmartroPaymentAdapter::~SmartroPaymentAdapter() {
    monitorRunning_ = false;
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    
    smartroComm_->stopResponseReceiver();
}

devices::DeviceInfo SmartroPaymentAdapter::getDeviceInfo() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    devices::DeviceInfo info;
    info.deviceId = deviceId_;
    info.deviceType = devices::DeviceType::PAYMENT_TERMINAL;
    info.deviceName = "SMARTRO Payment Terminal";
    info.state = state_;
    info.lastError = lastError_;
    info.lastUpdateTime = lastUpdateTime_;
    
    return info;
}

bool SmartroPaymentAdapter::startPayment(uint32_t amount) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (state_ != devices::DeviceState::STATE_READY) {
        lastError_ = "Device is not ready. Current state: " + devices::deviceStateToString(state_);
        logging::Logger::getInstance().warn("Cannot start payment: " + lastError_);
        return false;
    }
    
    if (paymentInProgress_) {
        lastError_ = "Payment already in progress";
        logging::Logger::getInstance().warn("Cannot start payment: " + lastError_);
        return false;
    }
    
    // Send payment approval request (async) - non-blocking
    // This matches test_integrated.cpp pattern: sendPaymentApprovalRequestAsync only
    PaymentApprovalRequest approvalReq;
    approvalReq.transactionType = 1; // Approval
    approvalReq.amount = amount;
    approvalReq.tax = 0;
    approvalReq.service = 0;
    approvalReq.installments = 0; // Lump sum
    approvalReq.signatureRequired = 1; // No signature
    
    if (!smartroComm_->sendPaymentApprovalRequestAsync(terminalId_, approvalReq)) {
        lastError_ = "Failed to send payment approval request: " + smartroComm_->getLastError();
        updateState(devices::DeviceState::STATE_ERROR);
        return false;
    }
    
    // Change to processing state immediately (non-blocking)
    paymentInProgress_ = true;
    paymentCancelled_ = false;  // Reset cancel flag
    currentAmount_ = amount;
    updateState(devices::DeviceState::STATE_PROCESSING);
    
    // Response will be processed in background thread (eventMonitorThread)
    
    return true;
}

bool SmartroPaymentAdapter::cancelPayment() {
    bool shouldCallCallback = false;
    devices::PaymentCancelledEvent event;
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        
        if (!paymentInProgress_) {
            lastError_ = "No payment in progress";
            return false;
        }
        
        // Set cancel flag first to prevent background thread from processing response
        paymentCancelled_ = true;
        
        // Update state immediately so startPayment() can be called again
        paymentInProgress_ = false;
        updateState(devices::DeviceState::STATE_READY);
        
        // Prepare callback event
        if (paymentCancelledCallback_) {
            event.state = devices::DeviceState::STATE_READY;
            shouldCallCallback = true;
        }
    }
    
    // Send cancellation command ('E' - Payment Wait request acts as cancel during processing)
    // This is non-blocking async operation - matches test_integrated.cpp pattern
    // Do this outside mutex to avoid blocking
    logging::Logger::getInstance().info("Sending payment cancellation command (E) to device...");
    
    PaymentWaitResponse cancelResp;
    bool commandSent = smartroComm_->sendPaymentWaitRequest(terminalId_, cancelResp, 3000);
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (!commandSent) {
            lastError_ = "Failed to send cancellation command: " + smartroComm_->getLastError();
            logging::Logger::getInstance().error("Cancel payment command failed: " + lastError_);
        } else {
            logging::Logger::getInstance().info("Payment cancelled successfully");
        }
    }
    
    // Call callback outside mutex to avoid deadlock
    if (shouldCallCallback && paymentCancelledCallback_) {
        paymentCancelledCallback_(event);
    }
    
    return commandSent;
}

devices::DeviceState SmartroPaymentAdapter::getState() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

bool SmartroPaymentAdapter::reset() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (!smartroComm_->sendResetRequest(terminalId_, 3000)) {
        lastError_ = "Failed to reset device: " + smartroComm_->getLastError();
        return false;
    }
    
    paymentInProgress_ = false;
    paymentCancelled_ = false;
    updateState(devices::DeviceState::STATE_READY);
    
    return true;
}

bool SmartroPaymentAdapter::checkDevice() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    updateState(devices::DeviceState::STATE_CONNECTING);
    
    // Open serial port
    if (!serialPort_->isOpen()) {
        if (!serialPort_->open(comPort_, 115200)) {
            lastError_ = "Failed to open serial port: " + comPort_;
            updateState(devices::DeviceState::DISCONNECTED);
            return false;
        }
    }
    
    // Start response receiver thread
    if (!monitorRunning_) {
        smartroComm_->startResponseReceiver();
        monitorRunning_ = true;
        monitorThread_ = std::thread(&SmartroPaymentAdapter::eventMonitorThread, this);
    }
    
    // Send device check request
    DeviceCheckResponse response;
    if (!smartroComm_->sendDeviceCheckRequest(terminalId_, response, 3000)) {
        lastError_ = "Device check failed: " + smartroComm_->getLastError();
        updateState(devices::DeviceState::STATE_ERROR);
        return false;
    }
    
    // Check response
    bool allOk = (response.cardModuleStatus == 'O' || response.cardModuleStatus == 'N') &&
                 (response.rfModuleStatus == 'O' || response.rfModuleStatus == 'N') &&
                 (response.vanServerStatus == 'O' || response.vanServerStatus == 'N') &&
                 (response.integrationServerStatus == 'O' || response.integrationServerStatus == 'N');
    
    if (allOk) {
        updateState(devices::DeviceState::STATE_READY);
        lastError_.clear();
        return true;
    } else {
        std::ostringstream oss;
        oss << "Device check failed: card=" << response.cardModuleStatus
            << ", rf=" << response.rfModuleStatus
            << ", van=" << response.vanServerStatus
            << ", integration=" << response.integrationServerStatus;
        lastError_ = oss.str();
        updateState(devices::DeviceState::STATE_ERROR);
        return false;
    }
}

void SmartroPaymentAdapter::setPaymentCompleteCallback(std::function<void(const devices::PaymentCompleteEvent&)> callback) {
    paymentCompleteCallback_ = callback;
}

void SmartroPaymentAdapter::setPaymentFailedCallback(std::function<void(const devices::PaymentFailedEvent&)> callback) {
    paymentFailedCallback_ = callback;
}

void SmartroPaymentAdapter::setPaymentCancelledCallback(std::function<void(const devices::PaymentCancelledEvent&)> callback) {
    paymentCancelledCallback_ = callback;
}

void SmartroPaymentAdapter::setStateChangedCallback(std::function<void(devices::DeviceState)> callback) {
    stateChangedCallback_ = callback;
}

void SmartroPaymentAdapter::updateState(devices::DeviceState newState) {
    devices::DeviceState oldState = state_;
    state_ = newState;
    lastUpdateTime_ = std::chrono::system_clock::now();
    
    if (oldState != newState && stateChangedCallback_) {
        stateChangedCallback_(newState);
    }
}

void SmartroPaymentAdapter::processPaymentResponse(const PaymentApprovalResponse& response) {
    logging::Logger::getInstance().info("=== processPaymentResponse called ===");
    
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    // Check if payment was cancelled - ignore response if cancelled
    if (paymentCancelled_) {
        logging::Logger::getInstance().info("Ignoring payment response - payment was cancelled");
        paymentInProgress_ = false;
        paymentCancelled_ = false;
        // State should already be READY from cancelPayment()
        return;
    }
    
    // Double-check paymentInProgress_ in case it was cancelled between check and lock
    if (!paymentInProgress_) {
        logging::Logger::getInstance().warn("Ignoring payment response - payment not in progress (paymentInProgress_=false)");
        return;
    }
    
    paymentInProgress_ = false;
    
    if (response.isRejected()) {
        // Payment failed
        logging::Logger::getInstance().info("Payment was rejected");
        updateState(devices::DeviceState::STATE_READY);
        
        if (paymentFailedCallback_) {
            devices::PaymentFailedEvent event;
            event.errorCode = "VAN_REJECTED";
            event.errorMessage = response.rejectionInfo.empty() ? "Payment rejected" : response.rejectionInfo;
            event.amount = currentAmount_;
            event.state = devices::DeviceState::STATE_READY;
            logging::Logger::getInstance().info("Calling paymentFailedCallback_");
            paymentFailedCallback_(event);
        } else {
            logging::Logger::getInstance().warn("paymentFailedCallback_ is not set!");
        }
    } else {
        // Payment success
        logging::Logger::getInstance().info("Payment was successful - transactionId: " + response.transactionId);
        updateState(devices::DeviceState::STATE_READY);
        
        if (paymentCompleteCallback_) {
            devices::PaymentCompleteEvent event;
            event.transactionId = response.transactionId;
            event.amount = currentAmount_;
            event.cardNumber = response.cardNumber;
            event.approvalNumber = response.approvalNumber;
            event.salesDate = response.salesDate;
            event.salesTime = response.salesTime;
            event.transactionMedium = std::string(1, response.transactionMedium);
            event.state = devices::DeviceState::STATE_READY;
            logging::Logger::getInstance().info("Calling paymentCompleteCallback_");
            paymentCompleteCallback_(event);
        } else {
            logging::Logger::getInstance().warn("paymentCompleteCallback_ is not set!");
        }
    }
    
    logging::Logger::getInstance().info("=== processPaymentResponse completed ===");
}

void SmartroPaymentAdapter::processEvent(const EventResponse& event) {
    logging::Logger::getInstance().info("=== processEvent called - Event type: " + std::to_string(static_cast<int>(event.type)) + " ===");
    
    // Publish device state changed event for card detection events
    // This allows Flutter to know about card detection
    switch (event.type) {
        case EventType::IC_CARD_DETECTED:
            logging::Logger::getInstance().info("IC Card Detected event");
            if (stateChangedCallback_) {
                stateChangedCallback_(devices::DeviceState::STATE_PROCESSING);
            }
            break;
            
        case EventType::MS_CARD_DETECTED:
            logging::Logger::getInstance().info("MS Card Detected event");
            if (stateChangedCallback_) {
                stateChangedCallback_(devices::DeviceState::STATE_PROCESSING);
            }
            break;
            
        case EventType::RF_CARD_DETECTED:
            logging::Logger::getInstance().info("RF Card Detected event");
            if (stateChangedCallback_) {
                stateChangedCallback_(devices::DeviceState::STATE_PROCESSING);
            }
            break;
            
        case EventType::IC_CARD_REMOVED:
            logging::Logger::getInstance().info("IC Card Removed event");
            break;
            
        default:
            logging::Logger::getInstance().info("Unknown event type: " + std::to_string(static_cast<int>(event.type)));
            break;
    }
}

void SmartroPaymentAdapter::eventMonitorThread() {
    logging::Logger::getInstance().info("Event monitor thread started");
    
    while (monitorRunning_) {
        smartro::ResponseData response;
        if (smartroComm_->pollResponse(response, 1000)) {
            logging::Logger::getInstance().debug("Response received in eventMonitorThread, type: " + std::to_string(static_cast<int>(response.type)));
            
            switch (response.type) {
                case smartro::ResponseType::PAYMENT_APPROVAL:
                    logging::Logger::getInstance().info("Processing PAYMENT_APPROVAL response");
                    processPaymentResponse(response.paymentApproval);
                    break;
                    
                case smartro::ResponseType::EVENT:
                    logging::Logger::getInstance().info("Processing EVENT response");
                    processEvent(response.event);
                    break;
                    
                default:
                    logging::Logger::getInstance().debug("Unknown response type: " + std::to_string(static_cast<int>(response.type)));
                    break;
            }
        }
    }
    
    logging::Logger::getInstance().info("Event monitor thread exiting");
}

bool SmartroPaymentAdapter::readCardUid(CardUidReadResponse& response) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (!serialPort_->isOpen()) {
        lastError_ = "Serial port is not open";
        return false;
    }
    
    if (!smartroComm_->sendCardUidReadRequest(terminalId_, response, 3000)) {
        lastError_ = "Card UID read failed: " + smartroComm_->getLastError();
        return false;
    }
    
    return true;
}

bool SmartroPaymentAdapter::getLastApproval(LastApprovalResponse& response) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (!serialPort_->isOpen()) {
        lastError_ = "Serial port is not open";
        return false;
    }
    
    if (!smartroComm_->sendLastApprovalResponseRequest(terminalId_, response, 30000)) {
        lastError_ = "Last approval request failed: " + smartroComm_->getLastError();
        return false;
    }
    
    return true;
}

bool SmartroPaymentAdapter::checkIcCard(IcCardCheckResponse& response) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (!serialPort_->isOpen()) {
        lastError_ = "Serial port is not open";
        return false;
    }
    
    if (!smartroComm_->sendIcCardCheckRequest(terminalId_, response, 3000)) {
        lastError_ = "IC card check failed: " + smartroComm_->getLastError();
        return false;
    }
    
    return true;
}

bool SmartroPaymentAdapter::setScreenSound(const ScreenSoundSettingRequest& request, ScreenSoundSettingResponse& response) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (!serialPort_->isOpen()) {
        lastError_ = "Serial port is not open";
        return false;
    }
    
    if (!smartroComm_->sendScreenSoundSettingRequest(terminalId_, request, response, 3000)) {
        lastError_ = "Screen/sound setting failed: " + smartroComm_->getLastError();
        return false;
    }
    
    return true;
}

bool SmartroPaymentAdapter::cancelTransaction(const TransactionCancelRequest& request, TransactionCancelResponse& response) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    
    if (!serialPort_->isOpen()) {
        lastError_ = "Serial port is not open";
        return false;
    }
    
    if (!smartroComm_->sendTransactionCancelRequest(terminalId_, request, response, 30000)) {
        lastError_ = "Transaction cancel failed: " + smartroComm_->getLastError();
        return false;
    }
    
    return true;
}

} // namespace smartro
