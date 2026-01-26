// src/vendor_adapters/smartro/smartro_payment_terminal.cpp
#include "vendor_adapters/smartro/smartro_payment_terminal.h"
#include "common/uuid_generator.h"
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

namespace device_controller::vendor::smartro {

SMARTROPaymentTerminal::SMARTROPaymentTerminal()
    : state_(PaymentTerminalState::DISCONNECTED)
    , deviceId_("smartro_terminal_001")
    , deviceName_("SMARTRO Payment Terminal")
    , currentPaymentAmount_(0) {
}

SMARTROPaymentTerminal::~SMARTROPaymentTerminal() {
    shutdown();
}

PaymentTerminalState SMARTROPaymentTerminal::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

bool SMARTROPaymentTerminal::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }

    setState(PaymentTerminalState::CONNECTING);

    // Detect and connect to terminal
    if (!detectAndConnect()) {
        setState(PaymentTerminalState::ERROR);
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "CONNECTION_FAILED", 
                   "Failed to detect or connect to payment terminal");
        return false;
    }

    // Initialize protocol
    protocol_ = std::make_shared<SMARTROProtocol>(serialPort_);
    protocol_->setEventCallback([this](char eventCode, const std::string& data) {
        this->onProtocolEvent(eventCode, data);
    });

    if (!protocol_->initialize()) {
        setState(PaymentTerminalState::ERROR);
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "PROTOCOL_INIT_FAILED", 
                   "Failed to initialize protocol");
        return false;
    }

    // Perform device check
    if (!checkDeviceStatus()) {
        setState(PaymentTerminalState::ERROR);
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "DEVICE_CHECK_FAILED", 
                   "Device check failed");
        return false;
    }

    initialized_ = true;
    setState(PaymentTerminalState::READY);
    
    // Start monitoring thread
    monitoring_ = true;
    monitorThread_ = std::thread(&SMARTROPaymentTerminal::monitorConnection, this);

    notifyEvent(PaymentEventType::STATE_CHANGED);
    return true;
}

void SMARTROPaymentTerminal::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }

    monitoring_ = false;
    if (monitorThread_.joinable()) {
        lock.unlock();
        monitorThread_.join();
        lock.lock();
    }

    if (protocol_) {
        protocol_->shutdown();
        protocol_.reset();
    }

    if (serialPort_) {
        serialPort_->close();
        serialPort_.reset();
    }

    initialized_ = false;
    setState(PaymentTerminalState::DISCONNECTED);
}

bool SMARTROPaymentTerminal::startPayment(int64_t amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || state_ != PaymentTerminalState::READY) {
        return false;
    }

    if (amount <= 0) {
        return false;
    }

    currentPaymentAmount_ = amount;
    paymentInProgress_ = true;
    setState(PaymentTerminalState::PROCESSING);
    lastActivityTime_ = std::chrono::steady_clock::now();

    lock.unlock();

    // Send payment request
    // Default values: tax=0, serviceFee=0, installmentMonths=0, no signature
    bool sent = protocol_->sendPaymentRequest(amount, 0, 0, 0, false);

    lock.lock();

    if (!sent) {
        paymentInProgress_ = false;
        setState(PaymentTerminalState::ERROR);
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "PAYMENT_REQUEST_FAILED", 
                   "Failed to send payment request");
        return false;
    }

    notifyEvent(PaymentEventType::STATE_CHANGED);
    return true;
}

void SMARTROPaymentTerminal::cancelPayment() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !paymentInProgress_) {
        return;
    }

    lock.unlock();

    // Send cancel request (not last transaction cancel)
    protocol_->sendCancelRequest(false);

    lock.lock();

    paymentInProgress_ = false;
    setState(PaymentTerminalState::READY);
    notifyEvent(PaymentEventType::PAYMENT_CANCELLED);
}

bool SMARTROPaymentTerminal::reset() {
    return resetDevice();
}

void SMARTROPaymentTerminal::setEventCallback(PaymentEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = callback;
}

std::string SMARTROPaymentTerminal::getDeviceId() const {
    return deviceId_;
}

std::string SMARTROPaymentTerminal::getDeviceName() const {
    return deviceName_;
}

bool SMARTROPaymentTerminal::checkDeviceStatus() {
    if (!protocol_) {
        return false;
    }

    if (!protocol_->sendDeviceCheck()) {
        return false;
    }

    // Wait for response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    while (protocol_->isWaitingForResponse()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > 5000) {
            return false; // Timeout
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check device status
    auto status = protocol_->getLastDeviceStatus();
    
    // If any critical component is 'X' (error), device is not ready
    if (status.cardModuleStatus == 'X' || 
        status.vanServerStatus == 'X' || 
        status.integrationServerStatus == 'X') {
        return false;
    }

    return true;
}

bool SMARTROPaymentTerminal::resetDevice() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !protocol_) {
        return false;
    }

    lock.unlock();

    bool sent = protocol_->sendResetRequest();

    lock.lock();

    if (sent) {
        // Wait a bit for reset to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Re-check device status
        lock.unlock();
        bool statusOk = checkDeviceStatus();
        lock.lock();
        
        if (statusOk) {
            setState(PaymentTerminalState::READY);
        } else {
            setState(PaymentTerminalState::ERROR);
        }
    }

    return sent;
}

void SMARTROPaymentTerminal::setState(PaymentTerminalState newState) {
    if (state_ != newState) {
        state_ = newState;
    }
}

void SMARTROPaymentTerminal::notifyEvent(PaymentEventType type, const std::string& errorCode,
                                        const std::string& errorMessage) {
    if (!eventCallback_) {
        return;
    }

    PaymentEvent event;
    event.type = type;
    event.state = state_;
    event.errorCode = errorCode;
    event.errorMessage = errorMessage;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    if (type == PaymentEventType::PAYMENT_COMPLETE) {
        auto paymentResp = protocol_->getLastPaymentResponse();
        event.transactionId = paymentResp.transactionId;
        event.amount = paymentResp.amount;
    } else if (type == PaymentEventType::PAYMENT_FAILED) {
        event.amount = currentPaymentAmount_;
    }

    eventCallback_(event);
}

void SMARTROPaymentTerminal::onProtocolEvent(char eventCode, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    lastActivityTime_ = std::chrono::steady_clock::now();

    switch (eventCode) {
        case EVENT_MS_CARD:
        case EVENT_RF_CARD:
        case EVENT_IC_CARD:
            // Card detected - payment is progressing
            if (state_ == PaymentTerminalState::PROCESSING) {
                notifyEvent(PaymentEventType::STATE_CHANGED);
            }
            break;

        case EVENT_CARD_REMOVED:
            // Card removed - might be after payment
            break;

        case EVENT_FALLBACK:
            // Fallback occurred
            break;

        default:
            break;
    }

    // Check if payment response was received
    if (!protocol_->isWaitingForResponse() && paymentInProgress_) {
        auto paymentResp = protocol_->getLastPaymentResponse();
        
        if (!paymentResp.transactionId.empty()) {
            // Payment completed
            paymentInProgress_ = false;
            setState(PaymentTerminalState::READY);
            
            // Check VAN response code for success/failure
            if (paymentResp.vanResponseCode == "00" || paymentResp.vanResponseCode.empty()) {
                notifyEvent(PaymentEventType::PAYMENT_COMPLETE);
            } else {
                notifyEvent(PaymentEventType::PAYMENT_FAILED, 
                           "VAN_ERROR", 
                           "VAN response code: " + paymentResp.vanResponseCode);
            }
        }
    }
}

bool SMARTROPaymentTerminal::detectAndConnect() {
    // Try to detect terminal
    portName_ = SerialPort::detectTerminal();
    
    if (portName_.empty()) {
        // Try enumerating all ports and test each
        auto ports = SerialPort::enumeratePorts();
        for (const auto& port : ports) {
            auto testPort = std::make_shared<SerialPort>();
            if (testPort->open(port, 9600)) {
                // Try device check to verify it's a SMARTRO terminal
                auto testProtocol = std::make_shared<SMARTROProtocol>(testPort);
                if (testProtocol->initialize()) {
                    if (testProtocol->sendDeviceCheck()) {
                        // Wait a bit for response
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        if (!testProtocol->isWaitingForResponse()) {
                            // Got response, this might be our terminal
                            portName_ = port;
                            serialPort_ = testPort;
                            testProtocol->shutdown();
                            return true;
                        }
                    }
                    testProtocol->shutdown();
                }
                testPort->close();
            }
        }
        
        return false;
    }

    // Open detected port
    serialPort_ = std::make_shared<SerialPort>();
    if (!serialPort_->open(portName_, 9600)) {
        serialPort_.reset();
        return false;
    }

    return true;
}

void SMARTROPaymentTerminal::monitorConnection() {
    while (monitoring_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!initialized_) {
            continue;
        }

        // Check if port is still open
        if (!serialPort_ || !serialPort_->isOpen()) {
            setState(PaymentTerminalState::DISCONNECTED);
            lock.unlock();
            attemptReconnect();
            continue;
        }

        // Check for hung state
        if (checkHungState()) {
            setState(PaymentTerminalState::HUNG);
            notifyEvent(PaymentEventType::ERROR_OCCURRED, "HUNG_DETECTED", 
                       "Device appears to be hung");
            lock.unlock();
            attemptReconnect();
            continue;
        }

        // Periodic device status check
        static auto lastCheckTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckTime).count() > 30) {
            lastCheckTime = now;
            lock.unlock();
            checkDeviceStatus();
        }
    }
}

void SMARTROPaymentTerminal::attemptReconnect() {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // Backoff

    std::lock_guard<std::mutex> lock(mutex_);
    
    if (serialPort_) {
        serialPort_->close();
        serialPort_.reset();
    }

    if (protocol_) {
        protocol_->shutdown();
        protocol_.reset();
    }

    setState(PaymentTerminalState::CONNECTING);
    lock.unlock();

    if (detectAndConnect()) {
        lock.lock();
        protocol_ = std::make_shared<SMARTROProtocol>(serialPort_);
        protocol_->setEventCallback([this](char eventCode, const std::string& data) {
            this->onProtocolEvent(eventCode, data);
        });

        if (protocol_->initialize() && checkDeviceStatus()) {
            setState(PaymentTerminalState::READY);
            notifyEvent(PaymentEventType::STATE_CHANGED);
        } else {
            setState(PaymentTerminalState::ERROR);
        }
    } else {
        lock.lock();
        setState(PaymentTerminalState::DISCONNECTED);
    }
}

bool SMARTROPaymentTerminal::checkHungState() {
    if (state_ != PaymentTerminalState::PROCESSING) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - lastActivityTime_).count();

    return elapsed > HUNG_TIMEOUT_MS;
}

} // namespace device_controller::vendor::smartro
