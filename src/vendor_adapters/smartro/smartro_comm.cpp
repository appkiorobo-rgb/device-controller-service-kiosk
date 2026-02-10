// src/vendor_adapters/smartro/smartro_comm.cpp
// logger.h???????? include??? Windows SDK ?? ???
#include "logging/logger.h"
#include "vendor_adapters/smartro/smartro_comm.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace smartro {

SmartroComm::SmartroComm(SerialPort& serialPort)
    : serialPort_(serialPort)
    , state_(CommState::IDLE)
    , receiverRunning_(false) {
}

SmartroComm::~SmartroComm() {
    stopResponseReceiver();
}

bool SmartroComm::sendDeviceCheckRequest(const std::string& terminalId,
                                         DeviceCheckResponse& response,
                                         uint32_t /*timeoutMs*/,
                                         const std::string& preferredPort) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();

    if (serialPort_.isOpen()) {
        serialPort_.close();
    }

    std::vector<std::string> availablePorts = SerialPort::getAvailablePorts();

    if (availablePorts.empty()) {
        setError("No COM ports available");
        state_ = CommState::ERROR;
        return false;
    }

    if (!preferredPort.empty()) {
        auto it = std::find(availablePorts.begin(), availablePorts.end(), preferredPort);
        if (it != availablePorts.end()) {
            std::rotate(availablePorts.begin(), it, it + 1);
            logging::Logger::getInstance().info("Device check: Trying preferred port " + preferredPort + " first");
        }
    }
    if (preferredPort.empty()) {
        logging::Logger::getInstance().info("Device check: Testing all available COM ports");
    }
    
    // ??????????????????
    std::string currentPort;
    std::vector<std::string> triedPorts;
    
    for (const auto& portToTry : availablePorts) {
        // ??? ??? ???
        if (serialPort_.isOpen()) {
            serialPort_.close();
        }
        
        logging::Logger::getInstance().info("Testing port: " + portToTry);
        
        // ??? ??? ???
        bool portOpened = false;
        try {
            portOpened = serialPort_.open(portToTry, 115200);  // ?? ??????????
        } catch (...) {
            logging::Logger::getInstance().warn("Exception while opening port: " + portToTry);
            portOpened = false;
        }
        
        if (!portOpened) {
            logging::Logger::getInstance().warn("Failed to open port: " + portToTry + ", trying next port...");
            triedPorts.push_back(portToTry);
            continue;
        }
        
        currentPort = portToTry;
        triedPorts.push_back(currentPort);
        
        // ??? ???
        auto packet = SmartroProtocol::createDeviceCheckRequest(terminalId);
        
        // ??? ???
        state_ = CommState::SENDING_REQUEST;
        logging::Logger::getInstance().debug("Sending device check request on " + currentPort + "...");
        
        // ??? ??? ??? ?? ????
        flushSerialBuffer();
        
        if (!serialPort_.write(packet.data(), packet.size())) {
            logging::Logger::getInstance().warn("Failed to send request packet on " + currentPort);
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        // ACK ????(??? ??? ????? ??????????)
        state_ = CommState::WAITING_ACK;
        logging::Logger::getInstance().debug("Waiting for ACK on " + currentPort + "...");
        
        // ??? ??? ???????? ?????????? (1.5??
        uint32_t ackTimeout = 1500;
        
        std::vector<uint8_t> responsePacket;
        if (!waitForAck(ackTimeout, responsePacket)) {
            logging::Logger::getInstance().warn("ACK timeout or NACK received on " + currentPort + " (timeout: " + std::to_string(ackTimeout) + "ms)");
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        // ??? ??? (??? ??? ????? ??????????)
        state_ = CommState::RECEIVING_RESPONSE;
        logging::Logger::getInstance().debug("Receiving response on " + currentPort + "...");
        
        // ??? ??? ???????? ?????????? (2??
        uint32_t responseTimeout = 2000;
        
        if (!receiveResponse(responsePacket, responseTimeout)) {
            logging::Logger::getInstance().warn("Failed to receive response on " + currentPort + " (timeout: " + std::to_string(responseTimeout) + "ms)");
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        // ??? ???
        std::vector<uint8_t> header;
        std::vector<uint8_t> payload;
        
        if (responsePacket.empty()) {
            logging::Logger::getInstance().warn("Empty response packet on " + currentPort);
            sendNack();
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                         header, payload)) {
            logging::Logger::getInstance().warn("Failed to parse response on " + currentPort);
            sendNack();
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        // Job Code ???
        if (header.size() < HEADER_SIZE) {
            logging::Logger::getInstance().warn("Invalid header size on " + currentPort);
            sendNack();
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        char jobCode = SmartroProtocol::extractJobCode(header.data());
        if (jobCode != JOB_CODE_DEVICE_CHECK_RESPONSE) {
            logging::Logger::getInstance().warn("Unexpected job code on " + currentPort + ": " + std::string(1, jobCode));
            sendNack();
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        // ??? ????????
        if (!SmartroProtocol::parseDeviceCheckResponse(payload.data(), payload.size(), response)) {
            logging::Logger::getInstance().warn("Failed to parse device check response on " + currentPort);
            sendNack();
            serialPort_.close();  // ??? ???
            currentPort.clear();  // ??? ??? ???????? ????
            continue;
        }
        
        // ???! ACK ?????? ??? ????
        state_ = CommState::SENDING_ACK;
        if (!sendAck()) {
            logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
        }
        
        // ???????? ????
        SerialPort::saveWorkingPort(currentPort);
        logging::Logger::getInstance().info("Device check successful on port: " + currentPort);
        
        state_ = CommState::COMPLETED;
        logging::Logger::getInstance().debug("Device check request completed successfully");
        return true;
    }
    
    // ?? ??? ??? ???
    if (serialPort_.isOpen()) {
        serialPort_.close();
    }
    setError("Device check failed on all attempted ports: " + std::to_string(triedPorts.size()) + " ports tried");
    state_ = CommState::ERROR;
    return false;
}

bool smartro::SmartroComm::sendPaymentWaitRequest(const std::string& terminalId, 
                                                  PaymentWaitResponse& response,
                                                  uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ??? ???
    auto packet = SmartroProtocol::createPaymentWaitRequest(terminalId);
    
    // ??? ??? (???????? 1?? ???)
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending payment wait request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? ??? ?? ????(??? ????????)
    flushSerialBuffer();
    
    // ACK ????(ACK ??? ???? ??? ?????????????????? ??)
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? (???? STX???????????)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ???
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (responsePacket.empty()) {
        setError("Empty response packet");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                     header, payload)) {
        setError("Failed to parse response");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ???
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_PAYMENT_WAIT_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_PAYMENT_WAIT_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ????????
    if (!SmartroProtocol::parsePaymentWaitResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse payment wait response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???! ACK ?????? ?? ??
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Payment wait request completed successfully");
    return true;
}

bool SmartroComm::sendCardUidReadRequest(const std::string& terminalId, 
                                         CardUidReadResponse& response,
                                         uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ??? ???
    auto packet = SmartroProtocol::createCardUidReadRequest(terminalId);
    
    // ??? ??? (???????? 1?? ???)
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending card UID read request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? ??? ?? ????(??? ????????)
    flushSerialBuffer();
    
    // ACK ????(ACK ??? ???? ??? ?????????????????? ??)
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? (???? STX???????????)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ???
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (responsePacket.empty()) {
        setError("Empty response packet");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                     header, payload)) {
        setError("Failed to parse response");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ???
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_CARD_UID_READ_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_CARD_UID_READ_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ????????
    if (!SmartroProtocol::parseCardUidReadResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse card UID read response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???! ACK ?????? ?? ??
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Card UID read request completed successfully");
    return true;
}

bool SmartroComm::waitForEvent(EventResponse& event, uint32_t timeoutMs) {
    // ????????? ??? ???????????????????????? ????? ???????
    // STX???? ?????? ??? ????? ??? ?????????????? ??
    
    if (!serialPort_.isOpen()) {
        std::lock_guard<std::mutex> lock(commMutex_);
        setError("Serial port is not open");
        return false;
    }
    
    // ?????????(?????? ????? ?????? ???)
    logging::Logger::getInstance().debug("Waiting for event...");
    
    std::vector<uint8_t> eventPacket;
    
    // STX??????(??????????: 0??? ?? ????
    // ??? ???????????????????? ????? ???????
    uint32_t stxTimeout = 100;  // 100ms????
    uint32_t totalElapsed = 0;
    
    while (timeoutMs == 0 || totalElapsed < timeoutMs) {
        uint8_t byte = 0;
        bool foundStx = false;
        
        // ??? ??? ???????? ??? ??? ???
        {
            std::lock_guard<std::mutex> lock(commMutex_);
            if (readByte(byte, stxTimeout)) {
                if (byte == STX) {
                    eventPacket.push_back(STX);
                    logging::Logger::getInstance().debug("STX received, reading event packet...");
                    foundStx = true;
                }
            }
        }
        
        if (foundStx) {
            break;
        }
        
        // ???????(?? ????????????? ???)
        if (timeoutMs == 0) {
            continue;  // ?? ????????????? ???
        }
        // ???????? ???????
        totalElapsed += stxTimeout;
        if (totalElapsed >= timeoutMs) {
            std::lock_guard<std::mutex> lock(commMutex_);
            setError("Timeout waiting for event");
            state_ = CommState::ERROR;
            return false;
        }
    }
    
    // STX??????? ??? ??? ??? ???
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    state_ = CommState::RECEIVING_RESPONSE;
    
    // ???????? ??? (STX?????? ???????????? ???)
    if (!receiveResponse(eventPacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive event packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???????? ???
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (eventPacket.empty()) {
        setError("Empty event packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(eventPacket.data(), eventPacket.size(), 
                                     header, payload)) {
        setError("Failed to parse event packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ???
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_EVENT) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_EVENT));
        state_ = CommState::ERROR;
        return false;
    }
    
    // ?????????????
    if (!SmartroProtocol::parseEventResponse(payload.data(), payload.size(), event)) {
        setError("Failed to parse event response data");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???! ?????? ACK/NACK ??????? ???
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Event received successfully");
    return true;
}

bool SmartroComm::sendResetRequest(const std::string& terminalId, uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ??? ???
    auto packet = SmartroProtocol::createResetRequest(terminalId);
    
    // ??? ???
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending reset request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? ??? ?? ????
    flushSerialBuffer();
    
    // ACK ????
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? (?? ????? ?????????)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? (?? ????? Job Code?????)
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (responsePacket.empty()) {
        setError("Empty response packet");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                     header, payload)) {
        setError("Failed to parse response");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ???
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_RESET_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_RESET_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???! ACK ???
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Reset request completed successfully");
    return true;
}

bool SmartroComm::sendPaymentApprovalRequest(const std::string& terminalId, 
                                            const PaymentApprovalRequest& request,
                                            PaymentApprovalResponse& response,
                                            uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(commMutex_);
    
    const uint32_t userInactivityTimeoutMs = 30000;  // 30??(?????, ?? 60000??? ???
    
    // ???? ???? ?????(??????? ?? ?? ???)
    while (true) {
        state_ = CommState::IDLE;
        lastError_.clear();
        
        if (!serialPort_.isOpen()) {
            setError("Serial port is not open");
            return false;
        }
        
        // ???? ????? 30???????????? ??? (??? ???)
        auto requestStartTime = std::chrono::steady_clock::now();
        logging::Logger::getInstance().info("Payment approval request started, 30s timeout begins");
        
        // ??? ???
        auto packet = SmartroProtocol::createPaymentApprovalRequest(terminalId, request);
        
        // ??? ???
        state_ = CommState::SENDING_REQUEST;
        logging::Logger::getInstance().debug("Sending payment approval request...");
        
        if (!serialPort_.write(packet.data(), packet.size())) {
            setError("Failed to send request packet");
            state_ = CommState::ERROR;
            return false;
        }
        
        // ??? ??? ??? ?? ????
        flushSerialBuffer();
        
        // ACK ????(30??????????????
        state_ = CommState::WAITING_ACK;
        logging::Logger::getInstance().debug("Waiting for ACK...");
        
        // ???? ?????????? ??
        auto elapsed = std::chrono::steady_clock::now() - requestStartTime;
        uint32_t elapsedMs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        
        uint32_t remainingSeconds = (userInactivityTimeoutMs - elapsedMs) / 1000;
        logging::Logger::getInstance().debug("Timeout check: elapsed=" + 
                                             std::to_string(elapsedMs / 1000) + "s, remaining=" + 
                                             std::to_string(remainingSeconds) + "s");
        
        uint32_t ackRemainingTimeout = (elapsedMs < userInactivityTimeoutMs) ? 
                                    (userInactivityTimeoutMs - elapsedMs) : 1000;  // ?? 1??
        
        // ACK ????????????? ???? ??????? ACK ????????????? ?????
        uint32_t ackTimeout = (ackRemainingTimeout < ACK_TIMEOUT_MS) ? ackRemainingTimeout : ACK_TIMEOUT_MS;
        
        std::vector<uint8_t> responsePacket;
        if (!waitForAck(ackTimeout, responsePacket)) {
            // ACK ?????????? ??? ?????????
            elapsed = std::chrono::steady_clock::now() - requestStartTime;
            elapsedMs = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
            
            if (elapsedMs >= userInactivityTimeoutMs) {
                // ??? ????????? - Payment Wait??? ???
                logging::Logger::getInstance().warn("Request timeout reached: elapsed=" + 
                                                   std::to_string(elapsedMs / 1000) + "s (limit=" + 
                                                   std::to_string(userInactivityTimeoutMs / 1000) + 
                                                   "s), sending Payment Wait");
                lock.unlock();
                PaymentWaitResponse waitResponse;
                bool waitSuccess = sendPaymentWaitRequest(terminalId, waitResponse, 3000);
                lock.lock();
                if (waitSuccess) {
                    logging::Logger::getInstance().info("Payment Wait sent successfully");
                }
                setError("User inactivity timeout");
                state_ = CommState::ERROR;
                return false;
            }
            
            setError("ACK timeout or NACK received");
            state_ = CommState::ERROR;
            return false;
        }
        
        // ??? ??? (30??????????????
        state_ = CommState::RECEIVING_RESPONSE;
        logging::Logger::getInstance().debug("Receiving response...");
        
        // ???? ?????????? ?????
        elapsed = std::chrono::steady_clock::now() - requestStartTime;
        elapsedMs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        
        remainingSeconds = (userInactivityTimeoutMs - elapsedMs) / 1000;
        logging::Logger::getInstance().debug("Timeout check before response: elapsed=" + 
                                             std::to_string(elapsedMs / 1000) + "s, remaining=" + 
                                             std::to_string(remainingSeconds) + "s");
        
        uint32_t remainingTimeout = (elapsedMs < userInactivityTimeoutMs) ? 
                          (userInactivityTimeoutMs - elapsedMs) : 1000;  // ?? 1??
        
        if (timeoutMs > 0 && timeoutMs < remainingTimeout) {
            remainingTimeout = timeoutMs;
        }
        
        if (!receiveResponse(responsePacket, remainingTimeout)) {
            // ????????? - ??? ?????????
            elapsed = std::chrono::steady_clock::now() - requestStartTime;
            elapsedMs = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
            
            if (elapsedMs >= userInactivityTimeoutMs) {
                // ??? ????????? - Payment Wait??? ??? (????????)
                logging::Logger::getInstance().warn("Request timeout reached: elapsed=" + 
                                                   std::to_string(elapsedMs / 1000) + "s (limit=" + 
                                                   std::to_string(userInactivityTimeoutMs / 1000) + 
                                                   "s), sending Payment Wait to reset state");
                
                // Payment Wait ?????? ??????
                lock.unlock();
                
                PaymentWaitResponse waitResponse;
                bool waitSuccess = sendPaymentWaitRequest(terminalId, waitResponse, 3000);
                
                lock.lock();
                
                if (waitSuccess) {
                    logging::Logger::getInstance().info("Payment Wait sent successfully, state reset");
                } else {
                    logging::Logger::getInstance().warn("Failed to send Payment Wait");
                }
                
                setError("User inactivity timeout");
                state_ = CommState::ERROR;
                return false;
            } else {
                // ??????????(??? ??? ???) - ????????
                setError("Failed to receive response");
                state_ = CommState::ERROR;
                return false;
            }
        }
        
        // ??? ???
        std::vector<uint8_t> header;
        std::vector<uint8_t> payload;
        
        if (responsePacket.empty()) {
            setError("Empty response packet");
            sendNack();
            state_ = CommState::ERROR;
            return false;
        }
        
        if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                         header, payload)) {
            setError("Failed to parse response");
            sendNack();
            state_ = CommState::ERROR;
            return false;
        }
        
        // Job Code ???
        if (header.size() < HEADER_SIZE) {
            setError("Invalid header size: " + std::to_string(header.size()));
            sendNack();
            state_ = CommState::ERROR;
            return false;
        }
        
        char jobCode = SmartroProtocol::extractJobCode(header.data());
        if (jobCode != JOB_CODE_PAYMENT_APPROVAL_RESPONSE) {
            setError("Unexpected job code: " + std::string(1, jobCode) + 
                    ", expected: " + std::string(1, JOB_CODE_PAYMENT_APPROVAL_RESPONSE));
            sendNack();
            state_ = CommState::ERROR;
            return false;
        }
        
        // ??? ????????
        if (!SmartroProtocol::parsePaymentApprovalResponse(payload.data(), payload.size(), response)) {
            setError("Failed to parse payment approval response data");
            sendNack();
            state_ = CommState::ERROR;
            return false;
        }
        
        // ???????? Transaction Medium????? ??? ??
        if (response.isRejected()) {
            // ??? ??????? ??? ??
            elapsed = std::chrono::steady_clock::now() - requestStartTime;
            elapsedMs = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
            
            // ACK ??? (????? ????????
            state_ = CommState::SENDING_ACK;
            if (!sendAck()) {
                logging::Logger::getInstance().warn("Failed to send ACK");
            }
            
            // Transaction Medium ???
            if (response.transactionMedium == '1') {
                // IC (?? ???)????: ?? ??? ??????? ????? ??
                // ??? ?????IPC/Flutter)??? IC_CARD_REMOVED ??????? ??????? ?? ???????????
                logging::Logger::getInstance().warn("Payment approval rejected (IC, elapsed=" + 
                                                   std::to_string(elapsedMs / 1000) + "s). " +
                                                   "Waiting for card removal event to retry...");
                setError("Payment rejected (IC). Card removal event required for retry");
                state_ = CommState::ERROR;
                return false;  // ??? ??, ??? ?????????? ??? ????????????
            } else if (response.transactionMedium == '3') {
                // RF (??)????: 3?????????????
                const uint32_t rfRetryDelayMs = 3000;  // 3??
                logging::Logger::getInstance().warn("Payment approval rejected (RF, elapsed=" + 
                                                   std::to_string(elapsedMs / 1000) + "s). " +
                                                   "Retrying after " + std::to_string(rfRetryDelayMs / 1000) + "s...");
                
                // 3??????(Flutter?? ???????????? ??? ??? ??)
                auto retryStartTime = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::milliseconds(rfRetryDelayMs));
                auto retryElapsed = std::chrono::steady_clock::now() - retryStartTime;
                uint32_t retryElapsedMs = static_cast<uint32_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(retryElapsed).count());
                logging::Logger::getInstance().info("RF retry delay completed: " + 
                                                   std::to_string(retryElapsedMs / 1000) + "s");
                
                continue;  // ?????(??? ????? ?????30????? ???)
            } else {
                // ??? ?? (MS, QR, KEYIN ??????: ?? ???????
                logging::Logger::getInstance().warn("Payment approval rejected (Medium=" + 
                                                   std::string(1, response.transactionMedium) + 
                                                   ", elapsed=" + std::to_string(elapsedMs / 1000) + 
                                                   "s), retrying with same amount...");
                
                // ??? ???????????
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;  // ?? ?????(??? ????? ?????30????? ???)
            }
        }
        
        // ???! ACK ???
        elapsed = std::chrono::steady_clock::now() - requestStartTime;
        elapsedMs = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        
        state_ = CommState::SENDING_ACK;
        if (!sendAck()) {
            logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
        }
        
        state_ = CommState::COMPLETED;
        logging::Logger::getInstance().info("Payment approval request completed successfully (elapsed=" + 
                                           std::to_string(elapsedMs / 1000) + "s)");
        return true;
    }
}

bool SmartroComm::sendLastApprovalResponseRequest(const std::string& terminalId, 
                                                  LastApprovalResponse& response,
                                                  uint32_t timeoutMs) {
    {
        std::lock_guard<std::mutex> lock(commMutex_);
        state_ = CommState::IDLE;
        lastError_.clear();
        
        if (!serialPort_.isOpen()) {
            setError("Serial port is not open");
            return false;
        }
        
        // Create request packet
        auto packet = SmartroProtocol::createLastApprovalResponseRequest(terminalId);
        
        // Send packet
        state_ = CommState::SENDING_REQUEST;
        logging::Logger::getInstance().debug("Sending last approval response request...");
        
        if (!serialPort_.write(packet.data(), packet.size())) {
            setError("Failed to send request packet");
            state_ = CommState::ERROR;
            return false;
        }
        
        // Flush serial buffer
        flushSerialBuffer();
        
        // Wait for ACK
        state_ = CommState::WAITING_ACK;
        logging::Logger::getInstance().debug("Waiting for ACK...");
        
        std::vector<uint8_t> responsePacket;
        if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
            setError("ACK timeout or NACK received");
            state_ = CommState::ERROR;
            return false;
        }
        
        // Send ACK (prepare for response reception)
        state_ = CommState::SENDING_ACK;
        if (!sendAck()) {
            logging::Logger::getInstance().warn("Failed to send ACK");
        }
    }
    
    // Response will be queued by responseReceiverThread, so get it from queue
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Waiting for last approval response from queue...");
    
    uint32_t actualTimeout = (timeoutMs == 0) ? (RESPONSE_TIMEOUT_MS * 3) : timeoutMs;
    ResponseData responseData;
    if (!pollResponse(responseData, actualTimeout)) {
        std::lock_guard<std::mutex> lock(commMutex_);
        setError("Timeout waiting for last approval response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // Verify Job Code
    if (responseData.jobCode != JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE) {
        std::lock_guard<std::mutex> lock(commMutex_);
        setError("Unexpected job code: " + std::string(1, responseData.jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE));
        state_ = CommState::ERROR;
        return false;
    }
    
    // Copy response data
    if (responseData.type != ResponseType::LAST_APPROVAL) {
        std::lock_guard<std::mutex> lock(commMutex_);
        setError("Unexpected response type");
        state_ = CommState::ERROR;
        return false;
    }
    
    response = responseData.lastApproval;
    
    {
        std::lock_guard<std::mutex> lock(commMutex_);
        state_ = CommState::COMPLETED;
    }
    
    logging::Logger::getInstance().info("Last approval response request completed successfully: " + 
                                       std::to_string(response.data.size()) + " bytes");
    return true;
}

bool SmartroComm::sendScreenSoundSettingRequest(const std::string& terminalId, 
                                                const ScreenSoundSettingRequest& request,
                                                ScreenSoundSettingResponse& response,
                                                uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ??? ???
    auto packet = SmartroProtocol::createScreenSoundSettingRequest(terminalId, request);
    
    // ??? ???
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending screen/sound setting request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? ??? ?? ????
    flushSerialBuffer();
    
    // ACK ????
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ???
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ???
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (responsePacket.empty()) {
        setError("Empty response packet");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                     header, payload)) {
        setError("Failed to parse response");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ???
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_SCREEN_SOUND_SETTING_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_SCREEN_SOUND_SETTING_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ????????
    if (!SmartroProtocol::parseScreenSoundSettingResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse screen/sound setting response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???! ACK ???
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Screen/sound setting request completed successfully");
    return true;
}

bool SmartroComm::sendIcCardCheckRequest(const std::string& terminalId, 
                                         IcCardCheckResponse& response,
                                         uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ??? ???
    auto packet = SmartroProtocol::createIcCardCheckRequest(terminalId);
    
    // ??? ???
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending IC card check request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ??? ??? ?? ????
    flushSerialBuffer();
    
    // ACK ????
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ???
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ???
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (responsePacket.empty()) {
        setError("Empty response packet");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                     header, payload)) {
        setError("Failed to parse response");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ???
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_IC_CARD_CHECK_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_IC_CARD_CHECK_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??? ????????
    if (!SmartroProtocol::parseIcCardCheckResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse IC card check response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ???! ACK ???
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("IC card check request completed successfully");
    return true;
}

bool SmartroComm::waitForAck(uint32_t timeoutMs, std::vector<uint8_t>& responsePacket) {
    responsePacket.clear();
    uint8_t byte = 0;
    
    if (!readByte(byte, timeoutMs)) {
        logging::Logger::getInstance().error("Timeout waiting for ACK/NACK");
        return false;
    }
    
    if (byte == ACK) {
        logging::Logger::getInstance().debug("ACK received (0x06)");
        
        // ACK????? ?? ??? ??????? ????????????? ??????????STX ???
        uint8_t nextByte = 0;
        if (readByte(nextByte, 1000)) {  // 1?????????(????????????????)
            if (nextByte == STX) {
                logging::Logger::getInstance().debug("STX received immediately after ACK");
                responsePacket.push_back(STX);
                return true;
            } else {
                // STX? ???????????? ??? ????????????????
                logging::Logger::getInstance().warn("Unexpected byte after ACK: 0x" + 
                                                   std::to_string(static_cast<int>(nextByte)));
            }
        }
        // STX? ???????ACK????????????? (receiveResponse??? STX???? ??
        return true;
    } else if (byte == NACK) {
        logging::Logger::getInstance().warn("NACK received (0x15)");
        return false;
    } else if (byte == STX) {
        // STX? ??? ??? (ACK ??? ??????? ???)
        logging::Logger::getInstance().debug("STX received instead of ACK, treating as response start");
        responsePacket.push_back(STX);
        return true;
    } else {
        logging::Logger::getInstance().warn("Unexpected byte received while waiting for ACK: 0x" + 
                                           std::to_string(static_cast<int>(byte)) + 
                                           ", discarding...");
        // ??????? ????? ?????????? ??????????????? ????
        uint8_t dummy;
        while (readByte(dummy, 10)) {
            // ?? ????
        }
        return false;
    }
}

bool SmartroComm::sendAck() {
    uint8_t ack = ACK;
    logging::Logger::getInstance().debug("Sending ACK (0x06)");
    
    if (!serialPort_.write(&ack, 1)) {
        logging::Logger::getInstance().error("Failed to send ACK");
        return false;
    }
    
    return true;
}

bool SmartroComm::sendNack() {
    uint8_t nack = NACK;
    logging::Logger::getInstance().debug("Sending NACK (0x15)");
    
    if (!serialPort_.write(&nack, 1)) {
        logging::Logger::getInstance().error("Failed to send NACK");
        return false;
    }
    
    return true;
}

bool SmartroComm::receiveResponse(std::vector<uint8_t>& responsePacket, uint32_t timeoutMs) {
    uint32_t elapsed = 0;
    const uint32_t readTimeout = 100;  // ????? ????? 100ms
    
    // STX ?? (???? ?????????)
    bool foundStx = !responsePacket.empty() && responsePacket[0] == STX;
    
    if (!foundStx) {
        while (elapsed < timeoutMs && !foundStx) {
            uint8_t byte = 0;
            if (readByte(byte, readTimeout)) {
                if (byte == STX) {
                    foundStx = true;
                    responsePacket.push_back(byte);
                    logging::Logger::getInstance().debug("STX found, reading packet...");
                    break;
                }
            }
            elapsed += readTimeout;
        }
        
        if (!foundStx) {
            logging::Logger::getInstance().error("STX not found within timeout");
            return false;
        }
    } else {
        logging::Logger::getInstance().debug("STX already received, continuing packet read...");
    }
    
    // Header ???? ??? (34 bytes)
    size_t headerRemaining = HEADER_SIZE - 1;
    while (headerRemaining > 0 && elapsed < timeoutMs) {
        uint8_t byte = 0;
        if (readByte(byte, readTimeout)) {
            responsePacket.push_back(byte);
            headerRemaining--;
        } else {
            elapsed += readTimeout;
        }
    }
    
    if (headerRemaining > 0) {
        logging::Logger::getInstance().error("Failed to read complete header");
        return false;
    }
    
    // ??? ??? ???
    if (responsePacket.size() < HEADER_SIZE) {
        logging::Logger::getInstance().error("Header size insufficient: " + 
                                            std::to_string(responsePacket.size()) + 
                                            " bytes, expected: " + 
                                            std::to_string(HEADER_SIZE) + " bytes");
        return false;
    }
    
    // Data Length ??
    uint16_t dataLength = SmartroProtocol::extractDataLength(responsePacket.data());
    logging::Logger::getInstance().debug("Response data length: " + std::to_string(dataLength));
    
    // Data ???
    for (uint16_t i = 0; i < dataLength && elapsed < timeoutMs; ++i) {
        uint8_t byte = 0;
        if (readByte(byte, readTimeout)) {
            responsePacket.push_back(byte);
        } else {
            elapsed += readTimeout;
        }
    }
    
    // ETX?? BCC ???
    uint8_t etx = 0;
    uint8_t bcc = 0;
    
    if (!readByte(etx, readTimeout) || etx != ETX) {
        logging::Logger::getInstance().error("Failed to read ETX");
        return false;
    }
    responsePacket.push_back(etx);
    
    if (!readByte(bcc, readTimeout)) {
        logging::Logger::getInstance().error("Failed to read BCC");
        return false;
    }
    responsePacket.push_back(bcc);
    
    logging::Logger::getInstance().debug("Response packet received: " + 
                                       std::to_string(responsePacket.size()) + " bytes");
    
    // ??? ????????? ?? ??
    if (responsePacket.size() > 0) {
        logging::Logger::getInstance().debugHex("Serial RX [Complete Packet]", 
                                               responsePacket.data(), responsePacket.size());
    }
    
    return true;
}

bool SmartroComm::readByte(uint8_t& byte, uint32_t timeoutMs) {
    size_t bytesRead = 0;
    if (!serialPort_.read(&byte, 1, bytesRead, timeoutMs)) {
        return false;
    }
    return bytesRead == 1;
}

void SmartroComm::flushSerialBuffer() {
    // ??? ???????????????????? ??????????????
    uint8_t dummy;
    size_t bytesRead = 0;
    while (serialPort_.read(&dummy, 1, bytesRead, 10) && bytesRead > 0) {
        // ?? ????
    }
    logging::Logger::getInstance().debug("Serial buffer flushed");
}

void SmartroComm::setError(const std::string& error) {
    lastError_ = error;
    logging::Logger::getInstance().error("SmartroComm error: " + error);
}

CommState SmartroComm::getState() const {
    return state_;
}

std::string SmartroComm::getLastError() const {
    return lastError_;
}

void SmartroComm::startResponseReceiver() {
    if (receiverRunning_.exchange(true)) {
        // ???? ??? ??
        return;
    }
    
    receiverThread_ = std::thread(&SmartroComm::responseReceiverThread, this);
    logging::Logger::getInstance().info("Response receiver thread started");
}

void SmartroComm::stopResponseReceiver() {
    if (!receiverRunning_.exchange(false)) {
        // ???? ?????
        return;
    }
    
    if (receiverThread_.joinable()) {
        queueCondition_.notify_all();  // ?????? ?????????
        receiverThread_.join();
    }
    
    logging::Logger::getInstance().info("Response receiver thread stopped");
}

void SmartroComm::responseReceiverThread() {
    logging::Logger::getInstance().debug("Response receiver thread started");
    
    while (receiverRunning_) {
        // STX ?? (??? ????????????)
        uint8_t byte = 0;
        bool foundStx = false;
        
        {
            std::lock_guard<std::mutex> lock(commMutex_);
            if (!serialPort_.isOpen()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            if (readByte(byte, 100)) {  // 100ms ???????
                if (byte == STX) {
                    foundStx = true;
                }
            }
        }
        
        if (!foundStx) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // STX??????? ??? ???
        std::vector<uint8_t> packet;
        packet.push_back(STX);
        
        {
            std::lock_guard<std::mutex> lock(commMutex_);
            if (!receiveResponse(packet, RESPONSE_TIMEOUT_MS)) {
                logging::Logger::getInstance().warn("Failed to receive response in receiver thread");
                continue;
            }
        }
        
        // ??? ?? ????? ???
        processResponse(packet);
    }
    
    logging::Logger::getInstance().debug("Response receiver thread exiting");
}

void SmartroComm::processResponse(const std::vector<uint8_t>& packet) {
    if (packet.empty()) {
        return;
    }
    
    // ??? ???
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (!SmartroProtocol::parsePacket(packet.data(), packet.size(), header, payload)) {
        logging::Logger::getInstance().warn("Failed to parse response packet in receiver thread");
        return;
    }
    
    if (header.size() < HEADER_SIZE) {
        logging::Logger::getInstance().warn("Invalid header size in receiver thread");
        return;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    
    ResponseData response;
    response.jobCode = jobCode;
    response.rawData = packet;
    
    // Job Code????? ???
    bool parsed = false;
    
    switch (jobCode) {
        case JOB_CODE_DEVICE_CHECK_RESPONSE:  // 'a'
            response.type = ResponseType::DEVICE_CHECK;
            parsed = SmartroProtocol::parseDeviceCheckResponse(payload.data(), payload.size(), 
                                                              response.deviceCheck);
            break;
            
        case JOB_CODE_PAYMENT_WAIT_RESPONSE:  // 'e'
            response.type = ResponseType::PAYMENT_WAIT;
            parsed = SmartroProtocol::parsePaymentWaitResponse(payload.data(), payload.size(), 
                                                              response.paymentWait);
            break;
            
        case JOB_CODE_CARD_UID_READ_RESPONSE:  // 'f'
            response.type = ResponseType::CARD_UID_READ;
            parsed = SmartroProtocol::parseCardUidReadResponse(payload.data(), payload.size(), 
                                                              response.cardUid);
            break;
            
        case JOB_CODE_RESET_RESPONSE:  // 'r'
            response.type = ResponseType::RESET;
            parsed = true;  // Reset?? ??????? ???
            break;
            
        case JOB_CODE_PAYMENT_APPROVAL_RESPONSE:  // 'b'
            response.type = ResponseType::PAYMENT_APPROVAL;
            parsed = SmartroProtocol::parsePaymentApprovalResponse(payload.data(), payload.size(), 
                                                                  response.paymentApproval);
            if (parsed) {
                logging::Logger::getInstance().info("Payment approval response parsed successfully: " + 
                                                   std::to_string(response.paymentApproval.data.size()) + " bytes");
            } else {
                logging::Logger::getInstance().error("Failed to parse payment approval response");
            }
            break;
            
        case JOB_CODE_TRANSACTION_CANCEL_RESPONSE:  // 'c'
            response.type = ResponseType::PAYMENT_APPROVAL;  // Same structure as payment approval
            parsed = SmartroProtocol::parsePaymentApprovalResponse(payload.data(), payload.size(), 
                                                                  response.paymentApproval);
            if (parsed) {
                logging::Logger::getInstance().info("Transaction cancel response parsed successfully: " + 
                                                   std::to_string(response.paymentApproval.data.size()) + " bytes");
            } else {
                logging::Logger::getInstance().error("Failed to parse transaction cancel response");
            }
            break;
            
        case JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE:  // 'l'
            response.type = ResponseType::LAST_APPROVAL;
            parsed = SmartroProtocol::parseLastApprovalResponse(payload.data(), payload.size(), 
                                                               response.lastApproval);
            if (parsed) {
                logging::Logger::getInstance().info("Last approval response parsed successfully: " + 
                                                   std::to_string(response.lastApproval.data.size()) + " bytes");
            } else {
                logging::Logger::getInstance().error("Failed to parse last approval response");
            }
            break;
            
        case JOB_CODE_SCREEN_SOUND_SETTING_RESPONSE:  // 's'
            response.type = ResponseType::SCREEN_SOUND_SETTING;
            parsed = SmartroProtocol::parseScreenSoundSettingResponse(payload.data(), payload.size(), 
                                                                     response.screenSound);
            break;
            
        case JOB_CODE_IC_CARD_CHECK_RESPONSE:  // 'm'
            response.type = ResponseType::IC_CARD_CHECK;
            parsed = SmartroProtocol::parseIcCardCheckResponse(payload.data(), payload.size(), 
                                                               response.icCard);
            break;
            
        case JOB_CODE_EVENT:  // '@'
            response.type = ResponseType::EVENT;
            parsed = SmartroProtocol::parseEventResponse(payload.data(), payload.size(), 
                                                        response.event);
            break;
            
        default:
            logging::Logger::getInstance().warn("Unknown job code in response: " + 
                                               std::string(1, jobCode));
            return;
    }
    
    if (!parsed) {
        logging::Logger::getInstance().warn("Failed to parse response data for job code: " + 
                                           std::string(1, jobCode));
        return;
    }
    
    // ??? ???
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        responseQueue_.push(response);
    }
    
    queueCondition_.notify_one();
    logging::Logger::getInstance().debug("Response queued: Job Code=" + std::string(1, jobCode));
}

bool SmartroComm::pollResponse(ResponseData& response, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(queueMutex_);

    if (timeoutMs == 0) {
        queueCondition_.wait(lock, [this] { return !responseQueue_.empty() || !receiverRunning_; });
    } else {
        auto timeout = std::chrono::milliseconds(timeoutMs);
        if (!queueCondition_.wait_for(lock, timeout,
                                      [this] { return !responseQueue_.empty() || !receiverRunning_; })) {
            return false;
        }
    }

    if (responseQueue_.empty()) {
        return false;
    }

    response = responseQueue_.front();
    responseQueue_.pop();
    return true;
}

bool SmartroComm::sendPaymentApprovalRequestAsync(const std::string& terminalId, 
                                                  const PaymentApprovalRequest& request) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ??? ???
    auto packet = SmartroProtocol::createPaymentApprovalRequest(terminalId, request);
    
    // ??? ???
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending payment approval request (async)...");
    
    // ??? ??? ??? ?? ????
    flushSerialBuffer();
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ACK ????(??? ???????
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ACK ???
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK");
    }
    
    // ??? ??? ??? (????? ?????????????????)
    state_ = CommState::IDLE;
    logging::Logger::getInstance().debug("Payment approval request sent (async), waiting for response in background");
    return true;
}

bool SmartroComm::sendTransactionCancelRequest(const std::string& terminalId,
                                               const TransactionCancelRequest& request,
                                               TransactionCancelResponse& response,
                                               uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // ?? ??
    auto packet = SmartroProtocol::createTransactionCancelRequest(terminalId, request);
    
    // ?? ??
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending transaction cancel request...");
    
    // ??? ?? ???
    flushSerialBuffer();
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ACK ??
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ?? ?? (STX? ?? ???)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, timeoutMs)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ?? ??
    std::vector<uint8_t> header;
    std::vector<uint8_t> payload;
    
    if (responsePacket.empty()) {
        setError("Empty response packet");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    if (!SmartroProtocol::parsePacket(responsePacket.data(), responsePacket.size(), 
                                     header, payload)) {
        setError("Failed to parse response");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // Job Code ??
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_TRANSACTION_CANCEL_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_TRANSACTION_CANCEL_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ?? ??? ??
    if (!SmartroProtocol::parseTransactionCancelResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse transaction cancel response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // ??! ACK ??
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Transaction cancel request completed successfully");
    return true;
}

} // namespace smartro
