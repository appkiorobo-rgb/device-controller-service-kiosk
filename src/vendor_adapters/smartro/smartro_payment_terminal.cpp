// src/vendor_adapters/smartro/smartro_payment_terminal.cpp
#include "vendor_adapters/smartro/smartro_payment_terminal.h"
#include "common/uuid_generator.h"
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>
#include <iostream>

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
    
    std::cout << "[PAYMENT] Starting initialization..." << std::endl;
    
    if (initialized_) {
        std::cout << "[PAYMENT] Already initialized" << std::endl;
        return true;
    }

    setState(PaymentTerminalState::CONNECTING);
    std::cout << "[PAYMENT] State: CONNECTING" << std::endl;

    // Detect and connect to terminal
    std::cout << "[PAYMENT] Detecting and connecting to terminal..." << std::endl;
    if (!detectAndConnect()) {
        setState(PaymentTerminalState::ERR);
        std::string errorMsg = "Failed to detect or connect to payment terminal";
        if (!portName_.empty()) {
            errorMsg += " (tried port: " + portName_ + ")";
        } else {
            errorMsg += " (no ports detected)";
        }
        std::cerr << "[PAYMENT] ERROR: " << errorMsg << std::endl;
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "CONNECTION_FAILED", errorMsg);
        return false;
    }
    
    std::cout << "[PAYMENT] Connected to port: " << portName_ << std::endl;

    // Initialize protocol
    std::cout << "[PAYMENT] Initializing protocol..." << std::endl;
    protocol_ = std::make_shared<SMARTROProtocol>(serialPort_);
    protocol_->setEventCallback([this](char eventCode, const std::string& data) {
        this->onProtocolEvent(eventCode, data);
    });

    if (!protocol_->initialize()) {
        setState(PaymentTerminalState::ERR);
        std::string errorMsg = "Failed to initialize protocol";
        if (!portName_.empty()) {
            errorMsg += " on port: " + portName_;
        }
        std::cerr << "[PAYMENT] ERROR: " << errorMsg << std::endl;
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "PROTOCOL_INIT_FAILED", errorMsg);
        return false;
    }
    
    std::cout << "[PAYMENT] Protocol initialized" << std::endl;

    // Perform device check
    std::cout << "[PAYMENT] Performing device check..." << std::endl;
    if (!checkDeviceStatus()) {
        setState(PaymentTerminalState::ERR);
        std::string errorMsg = "Device check failed";
        if (!portName_.empty()) {
            errorMsg += " on port: " + portName_;
        }
        auto status = protocol_->getLastDeviceStatus();
        errorMsg += " (Card:" + std::string(1, status.cardModuleStatus) + 
                    " RF:" + std::string(1, status.rfModuleStatus) +
                    " VAN:" + std::string(1, status.vanServerStatus) +
                    " Integration:" + std::string(1, status.integrationServerStatus) + ")";
        std::cerr << "[PAYMENT] ERROR: " << errorMsg << std::endl;
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "DEVICE_CHECK_FAILED", errorMsg);
        return false;
    }
    
    auto status = protocol_->getLastDeviceStatus();
    std::cout << "[PAYMENT] Device check OK - Card:" << status.cardModuleStatus 
              << " RF:" << status.rfModuleStatus 
              << " VAN:" << status.vanServerStatus 
              << " Integration:" << status.integrationServerStatus << std::endl;

    initialized_ = true;
    setState(PaymentTerminalState::READY);
    std::cout << "[PAYMENT] State: READY" << std::endl;
    
    // Start monitoring thread
    monitoring_ = true;
    monitorThread_ = std::thread(&SMARTROPaymentTerminal::monitorConnection, this);
    std::cout << "[PAYMENT] Monitoring thread started" << std::endl;

    notifyEvent(PaymentEventType::STATE_CHANGED);
    std::cout << "[PAYMENT] Initialization complete" << std::endl;
    return true;
}

void SMARTROPaymentTerminal::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    
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
    std::unique_lock<std::mutex> lock(mutex_);
    
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
        setState(PaymentTerminalState::ERR);
        notifyEvent(PaymentEventType::ERROR_OCCURRED, "PAYMENT_REQUEST_FAILED", 
                   "Failed to send payment request");
        return false;
    }

    notifyEvent(PaymentEventType::STATE_CHANGED);
    return true;
}

void SMARTROPaymentTerminal::cancelPayment() {
    std::unique_lock<std::mutex> lock(mutex_);
    
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
        return false; // Failed to send device check command
    }

    // Wait for response (with timeout)
    auto startTime = std::chrono::steady_clock::now();
    while (protocol_->isWaitingForResponse()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > 5000) {
            return false; // Timeout waiting for device check response
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
    std::unique_lock<std::mutex> lock(mutex_);
    
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
            setState(PaymentTerminalState::ERR);
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
        if (protocol_) {
            auto paymentResp = protocol_->getLastPaymentResponse();
            event.transactionId = paymentResp.transactionId;
            event.amount = paymentResp.amount;
        } else {
            event.amount = currentPaymentAmount_;
        }
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
    if (protocol_ && !protocol_->isWaitingForResponse() && paymentInProgress_) {
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
    // Flutter 구현 참고: 포트 우선순위 정렬 후 각 포트 시도
    std::cout << "[DETECT] Enumerating serial ports..." << std::endl;
    auto allPorts = SerialPort::enumeratePorts();
    
    if (allPorts.empty()) {
        std::cerr << "[DETECT] ERROR: No serial ports found" << std::endl;
        return false; // 사용 가능한 포트 없음
    }
    
    std::cout << "[DETECT] Found " << allPorts.size() << " port(s): ";
    for (size_t i = 0; i < allPorts.size(); i++) {
        std::cout << allPorts[i];
        if (i < allPorts.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    // 포트 우선순위 정렬 (Flutter: COM3, COM1, COM2, COM4...)
    std::vector<std::string> portPriority = {
        "COM3", "COM1", "COM2", "COM4", "COM5", 
        "COM6", "COM7", "COM8", "COM9", "COM10"
    };
    
    std::vector<std::string> sortedPorts;
    
    // 우선순위 포트 먼저 추가
    for (const auto& priorityPort : portPriority) {
        if (std::find(allPorts.begin(), allPorts.end(), priorityPort) != allPorts.end()) {
            sortedPorts.push_back(priorityPort);
        }
    }
    
    // 나머지 포트 추가
    for (const auto& port : allPorts) {
        if (std::find(sortedPorts.begin(), sortedPorts.end(), port) == sortedPorts.end()) {
            sortedPorts.push_back(port);
        }
    }
    
    std::cout << "[DETECT] Testing ports in order: ";
    for (size_t i = 0; i < sortedPorts.size(); i++) {
        std::cout << sortedPorts[i];
        if (i < sortedPorts.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    // 각 포트를 시도하면서 SMARTRO 단말기인지 확인
    for (size_t i = 0; i < sortedPorts.size(); i++) {
        const auto& port = sortedPorts[i];
        
        std::cout << "[DETECT] [" << (i + 1) << "/" << sortedPorts.size() << "] Testing port " << port << "..." << std::endl;
        
        // 포트 열기 시도
        auto testPort = std::make_shared<SerialPort>();
        if (!testPort->open(port, 115200)) {
            std::cout << "[DETECT] Failed to open port " << port << std::endl;
            continue;
        }
        
        std::cout << "[DETECT] Port " << port << " opened successfully" << std::endl;
        
        // 프로토콜 초기화 및 장치체크 요청
        std::cout << "[DETECT] Initializing protocol..." << std::endl;
        auto testProtocol = std::make_shared<SMARTROProtocol>(testPort);
        if (!testProtocol->initialize()) {
            std::cout << "[DETECT] Failed to initialize protocol" << std::endl;
            testPort->close();
            continue;
        }
        
        std::cout << "[DETECT] Sending device check request..." << std::endl;
        // 장치체크 요청 전송
        if (testProtocol->sendDeviceCheck()) {
            std::cout << "[DETECT] Device check sent, waiting for response..." << std::endl;
            // 응답 대기 (ACK를 받았으므로 응답 패킷을 기다림, 타임아웃 1초로 단축)
            // 실제 SMARTRO 장치는 빠르게 응답하므로 짧은 타임아웃으로 충분
            auto startTime = std::chrono::steady_clock::now();
            while (testProtocol->isWaitingForResponse()) {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > 1000) {
                    std::cout << "[DETECT] Timeout waiting for device check response (1 second)" << std::endl;
                    break; // 타임아웃
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 빠른 폴링 (10ms)
            }
            
            // 응답이 왔는지 확인
            if (!testProtocol->isWaitingForResponse()) {
                // 장치체크 응답 확인 - 실제 SMARTRO 단말기인지 검증
                auto status = testProtocol->getLastDeviceStatus();
                std::cout << "[DETECT] Device check response received - Card:" << status.cardModuleStatus 
                          << " RF:" << status.rfModuleStatus 
                          << " VAN:" << status.vanServerStatus 
                          << " Integration:" << status.integrationServerStatus << std::endl;
                
                // 최소한 하나의 모듈이 정상이면 SMARTRO 단말기로 간주
                if (status.cardModuleStatus == 'O' || status.rfModuleStatus == 'O' || 
                    status.vanServerStatus == 'O' || status.integrationServerStatus == 'O') {
                    // 올바른 단말기 발견
                    portName_ = port;
                    serialPort_ = testPort;
                    testProtocol->shutdown();
                    std::cout << "[DETECT] SMARTRO terminal found on port " << port << std::endl;
                    return true;
                } else {
                    std::cout << "[DETECT] Port " << port << " is not a SMARTRO terminal (all modules failed)" << std::endl;
                }
            } else {
                std::cout << "[DETECT] No response received from port " << port << " (timeout)" << std::endl;
            }
        } else {
            // sendDeviceCheck()가 false를 반환 = ACK 타임아웃 (500ms 경과)
            // 즉시 다음 포트로 넘어감 (추가 대기 없음)
            std::cout << "[DETECT] Failed to send device check to port " << port << " (ACK timeout 500ms - not a SMARTRO device)" << std::endl;
        }
        
        // 이 포트는 SMARTRO 단말기가 아님, 정리 후 다음 포트로
        testProtocol->shutdown();
        testPort->close();
    }
    
    // 모든 포트 시도 실패
    std::cerr << "[DETECT] ERROR: No SMARTRO terminal found on any port" << std::endl;
    return false;
}

void SMARTROPaymentTerminal::monitorConnection() {
    while (monitoring_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::unique_lock<std::mutex> lock(mutex_);
        
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

    std::unique_lock<std::mutex> lock(mutex_);
    
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
            setState(PaymentTerminalState::ERR);
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
