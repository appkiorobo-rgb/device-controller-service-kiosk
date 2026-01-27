// src/vendor_adapters/smartro/smartro_comm.cpp
#include "vendor_adapters/smartro/smartro_comm.h"
#include "logging/logger.h"

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
                                         uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // 패킷 생성
    auto packet = SmartroProtocol::createDeviceCheckRequest(terminalId);
    
    // 요청 전송 (재시도 없이 1번만 시도)
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending device check request...");
    
    // 요청 전송 전에 버퍼 비우기 (이전 데이터 제거)
    flushSerialBuffer();
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ACK 대기 (ACK 수신 후 바로 응답 패킷이 올 수 있으므로 함께 처리)
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신 (이미 STX를 받았을 수 있음)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_DEVICE_CHECK_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_DEVICE_CHECK_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parseDeviceCheckResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse device check response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송하고 즉시 반환
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Device check request completed successfully");
    return true;
}

bool SmartroComm::sendPaymentWaitRequest(const std::string& terminalId, 
                                         PaymentWaitResponse& response,
                                         uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // 패킷 생성
    auto packet = SmartroProtocol::createPaymentWaitRequest(terminalId);
    
    // 요청 전송 (재시도 없이 1번만 시도)
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending payment wait request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기 (이전 데이터 제거)
    flushSerialBuffer();
    
    // ACK 대기 (ACK 수신 후 바로 응답 패킷이 올 수 있으므로 함께 처리)
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신 (이미 STX를 받았을 수 있음)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
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
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parsePaymentWaitResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse payment wait response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송하고 즉시 반환
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
    
    // 패킷 생성
    auto packet = SmartroProtocol::createCardUidReadRequest(terminalId);
    
    // 요청 전송 (재시도 없이 1번만 시도)
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending card UID read request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기 (이전 데이터 제거)
    flushSerialBuffer();
    
    // ACK 대기 (ACK 수신 후 바로 응답 패킷이 올 수 있으므로 함께 처리)
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신 (이미 STX를 받았을 수 있음)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
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
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parseCardUidReadResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse card UID read response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송하고 즉시 반환
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Card UID read request completed successfully");
    return true;
}

bool SmartroComm::waitForEvent(EventResponse& event, uint32_t timeoutMs) {
    // 이벤트 대기는 다른 명령과 동시에 실행될 수 있도록 락을 풀었다 잡았다 함
    // STX를 찾는 동안에는 락을 풀어서 다른 명령이 실행될 수 있게 함
    
    if (!serialPort_.isOpen()) {
        std::lock_guard<std::mutex> lock(commMutex_);
        setError("Serial port is not open");
        return false;
    }
    
    // 이벤트 대기 (이벤트는 기기에서 자동으로 전송)
    logging::Logger::getInstance().debug("Waiting for event...");
    
    std::vector<uint8_t> eventPacket;
    
    // STX를 기다림 (타임아웃 설정: 0이면 무한 대기)
    // 짧은 타임아웃으로 반복하면서 락을 풀었다 잡았다 함
    uint32_t stxTimeout = 100;  // 100ms씩 체크
    uint32_t totalElapsed = 0;
    
    while (timeoutMs == 0 || totalElapsed < timeoutMs) {
        uint8_t byte = 0;
        bool foundStx = false;
        
        // 짧은 시간 동안만 락을 잡고 읽기 시도
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
        
        // 타임아웃 (무한 대기 모드에서는 계속 시도)
        if (timeoutMs == 0) {
            continue;  // 무한 대기 모드에서는 계속 시도
        }
        // 타임아웃이 설정된 경우
        totalElapsed += stxTimeout;
        if (totalElapsed >= timeoutMs) {
            std::lock_guard<std::mutex> lock(commMutex_);
            setError("Timeout waiting for event");
            state_ = CommState::ERROR;
            return false;
        }
    }
    
    // STX를 찾았으면 락을 잡고 패킷 읽기
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    state_ = CommState::RECEIVING_RESPONSE;
    
    // 이벤트 패킷 수신 (STX를 이미 받았으므로 나머지 읽기)
    if (!receiveResponse(eventPacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive event packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 이벤트 패킷 파싱
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
    
    // Job Code 확인
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
    
    // 이벤트 데이터 파싱
    if (!SmartroProtocol::parseEventResponse(payload.data(), payload.size(), event)) {
        setError("Failed to parse event response data");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! 이벤트는 ACK/NACK 전송하지 않음
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
    
    // 패킷 생성
    auto packet = SmartroProtocol::createResetRequest(terminalId);
    
    // 요청 전송
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending reset request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기
    flushSerialBuffer();
    
    // ACK 대기
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신 (리셋 응답은 간단할 수 있음)
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱 (리셋 응답은 Job Code만 확인)
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
    
    // Job Code 확인
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
    
    // 성공! ACK 전송
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
                                            uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // 패킷 생성
    auto packet = SmartroProtocol::createPaymentApprovalRequest(terminalId, request);
    
    // 요청 전송
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending payment approval request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기
    flushSerialBuffer();
    
    // ACK 대기
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS * 3)) {  // 결제는 더 오래 걸릴 수 있음
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
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
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parsePaymentApprovalResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse payment approval response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Payment approval request completed successfully");
    return true;
}

bool SmartroComm::sendLastApprovalResponseRequest(const std::string& terminalId, 
                                                  LastApprovalResponse& response,
                                                  uint32_t /*timeoutMs*/) {
    std::lock_guard<std::mutex> lock(commMutex_);
    state_ = CommState::IDLE;
    lastError_.clear();
    
    if (!serialPort_.isOpen()) {
        setError("Serial port is not open");
        return false;
    }
    
    // 패킷 생성
    auto packet = SmartroProtocol::createLastApprovalResponseRequest(terminalId);
    
    // 요청 전송
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending last approval response request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기
    flushSerialBuffer();
    
    // ACK 대기
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS * 3)) {  // 결제 응답은 더 오래 걸릴 수 있음
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
    if (header.size() < HEADER_SIZE) {
        setError("Invalid header size: " + std::to_string(header.size()));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    char jobCode = SmartroProtocol::extractJobCode(header.data());
    if (jobCode != JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE) {
        setError("Unexpected job code: " + std::string(1, jobCode) + 
                ", expected: " + std::string(1, JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE));
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parseLastApprovalResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse last approval response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK, but response was valid");
    }
    
    state_ = CommState::COMPLETED;
    logging::Logger::getInstance().debug("Last approval response request completed successfully");
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
    
    // 패킷 생성
    auto packet = SmartroProtocol::createScreenSoundSettingRequest(terminalId, request);
    
    // 요청 전송
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending screen/sound setting request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기
    flushSerialBuffer();
    
    // ACK 대기
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
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
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parseScreenSoundSettingResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse screen/sound setting response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송
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
    
    // 패킷 생성
    auto packet = SmartroProtocol::createIcCardCheckRequest(terminalId);
    
    // 요청 전송
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending IC card check request...");
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 요청 전송 전에 버퍼 비우기
    flushSerialBuffer();
    
    // ACK 대기
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 수신
    state_ = CommState::RECEIVING_RESPONSE;
    logging::Logger::getInstance().debug("Receiving response...");
    
    if (!receiveResponse(responsePacket, RESPONSE_TIMEOUT_MS)) {
        setError("Failed to receive response");
        state_ = CommState::ERROR;
        return false;
    }
    
    // 응답 파싱
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
    
    // Job Code 확인
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
    
    // 응답 데이터 파싱
    if (!SmartroProtocol::parseIcCardCheckResponse(payload.data(), payload.size(), response)) {
        setError("Failed to parse IC card check response data");
        sendNack();
        state_ = CommState::ERROR;
        return false;
    }
    
    // 성공! ACK 전송
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
        
        // ACK를 받은 후, 응답 패킷이 바로 올 수 있으므로 짧은 타임아웃으로 STX 확인
        uint8_t nextByte = 0;
        if (readByte(nextByte, 1000)) {  // 1초 타임아웃 (응답이 빠르게 올 수 있음)
            if (nextByte == STX) {
                logging::Logger::getInstance().debug("STX received immediately after ACK");
                responsePacket.push_back(STX);
                return true;
            } else {
                // STX가 아니면 버퍼에 다시 넣을 수 없으므로 로그만 남김
                logging::Logger::getInstance().warn("Unexpected byte after ACK: 0x" + 
                                                   std::to_string(static_cast<int>(nextByte)));
            }
        }
        // STX가 안 왔어도 ACK는 받았으므로 성공 (receiveResponse에서 STX를 찾을 것)
        return true;
    } else if (byte == NACK) {
        logging::Logger::getInstance().warn("NACK received (0x15)");
        return false;
    } else if (byte == STX) {
        // STX가 먼저 오면 (ACK 없이 응답이 바로 시작)
        logging::Logger::getInstance().debug("STX received instead of ACK, treating as response start");
        responsePacket.push_back(STX);
        return true;
    } else {
        logging::Logger::getInstance().warn("Unexpected byte received while waiting for ACK: 0x" + 
                                           std::to_string(static_cast<int>(byte)) + 
                                           ", discarding...");
        // 예상치 못한 바이트도 버퍼에 남아있을 수 있으므로 읽어서 버퍼 비우기
        uint8_t dummy;
        while (readByte(dummy, 10)) {
            // 버퍼 비우기
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
    const uint32_t readTimeout = 100;  // 각 읽기 시도마다 100ms
    
    // STX 찾기 (이미 받았을 수 있음)
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
    
    // Header 나머지 읽기 (34 bytes)
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
    
    // 헤더 크기 확인
    if (responsePacket.size() < HEADER_SIZE) {
        logging::Logger::getInstance().error("Header size insufficient: " + 
                                            std::to_string(responsePacket.size()) + 
                                            " bytes, expected: " + 
                                            std::to_string(HEADER_SIZE) + " bytes");
        return false;
    }
    
    // Data Length 추출
    uint16_t dataLength = SmartroProtocol::extractDataLength(responsePacket.data());
    logging::Logger::getInstance().debug("Response data length: " + std::to_string(dataLength));
    
    // Data 읽기
    for (uint16_t i = 0; i < dataLength && elapsed < timeoutMs; ++i) {
        uint8_t byte = 0;
        if (readByte(byte, readTimeout)) {
            responsePacket.push_back(byte);
        } else {
            elapsed += readTimeout;
        }
    }
    
    // ETX와 BCC 읽기
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
    // 짧은 타임아웃으로 버퍼에 남아있는 데이터 읽어서 버리기
    uint8_t dummy;
    size_t bytesRead = 0;
    while (serialPort_.read(&dummy, 1, bytesRead, 10) && bytesRead > 0) {
        // 버퍼 비우기
    }
    logging::Logger::getInstance().debug("Serial buffer flushed");
}

void SmartroComm::setError(const std::string& error) {
    lastError_ = error;
    logging::Logger::getInstance().error("SmartroComm error: " + error);
}

void SmartroComm::startResponseReceiver() {
    if (receiverRunning_.exchange(true)) {
        // 이미 실행 중
        return;
    }
    
    receiverThread_ = std::thread(&SmartroComm::responseReceiverThread, this);
    logging::Logger::getInstance().info("Response receiver thread started");
}

void SmartroComm::stopResponseReceiver() {
    if (!receiverRunning_.exchange(false)) {
        // 이미 중지됨
        return;
    }
    
    if (receiverThread_.joinable()) {
        queueCondition_.notify_all();  // 대기 중인 스레드 깨우기
        receiverThread_.join();
    }
    
    logging::Logger::getInstance().info("Response receiver thread stopped");
}

void SmartroComm::responseReceiverThread() {
    logging::Logger::getInstance().debug("Response receiver thread started");
    
    while (receiverRunning_) {
        // STX 찾기 (짧은 타임아웃으로 반복)
        uint8_t byte = 0;
        bool foundStx = false;
        
        {
            std::lock_guard<std::mutex> lock(commMutex_);
            if (!serialPort_.isOpen()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            if (readByte(byte, 100)) {  // 100ms 타임아웃
                if (byte == STX) {
                    foundStx = true;
                }
            }
        }
        
        if (!foundStx) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // STX를 찾았으면 패킷 읽기
        std::vector<uint8_t> packet;
        packet.push_back(STX);
        
        {
            std::lock_guard<std::mutex> lock(commMutex_);
            if (!receiveResponse(packet, RESPONSE_TIMEOUT_MS)) {
                logging::Logger::getInstance().warn("Failed to receive response in receiver thread");
                continue;
            }
        }
        
        // 응답 처리 및 큐에 추가
        processResponse(packet);
    }
    
    logging::Logger::getInstance().debug("Response receiver thread exiting");
}

void SmartroComm::processResponse(const std::vector<uint8_t>& packet) {
    if (packet.empty()) {
        return;
    }
    
    // 패킷 파싱
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
    
    // Job Code에 따라 파싱
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
            parsed = true;  // Reset은 데이터가 없음
            break;
            
        case JOB_CODE_PAYMENT_APPROVAL_RESPONSE:  // 'b'
            response.type = ResponseType::PAYMENT_APPROVAL;
            parsed = SmartroProtocol::parsePaymentApprovalResponse(payload.data(), payload.size(), 
                                                                  response.paymentApproval);
            break;
            
        case JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE:  // 'l'
            response.type = ResponseType::LAST_APPROVAL;
            parsed = SmartroProtocol::parseLastApprovalResponse(payload.data(), payload.size(), 
                                                               response.lastApproval);
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
    
    // 큐에 추가
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
        // 무한 대기
        queueCondition_.wait(lock, [this] { return !responseQueue_.empty() || !receiverRunning_; });
    } else {
        // 타임아웃 설정
        auto timeout = std::chrono::milliseconds(timeoutMs);
        if (!queueCondition_.wait_for(lock, timeout, 
                                      [this] { return !responseQueue_.empty() || !receiverRunning_; })) {
            return false;  // 타임아웃
        }
    }
    
    if (responseQueue_.empty()) {
        return false;  // 스레드가 중지됨
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
    
    // 패킷 생성
    auto packet = SmartroProtocol::createPaymentApprovalRequest(terminalId, request);
    
    // 요청 전송
    state_ = CommState::SENDING_REQUEST;
    logging::Logger::getInstance().debug("Sending payment approval request (async)...");
    
    // 요청 전송 전에 버퍼 비우기
    flushSerialBuffer();
    
    if (!serialPort_.write(packet.data(), packet.size())) {
        setError("Failed to send request packet");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ACK 대기 (짧은 타임아웃)
    state_ = CommState::WAITING_ACK;
    logging::Logger::getInstance().debug("Waiting for ACK...");
    
    std::vector<uint8_t> responsePacket;
    if (!waitForAck(ACK_TIMEOUT_MS, responsePacket)) {
        setError("ACK timeout or NACK received");
        state_ = CommState::ERROR;
        return false;
    }
    
    // ACK 전송
    state_ = CommState::SENDING_ACK;
    if (!sendAck()) {
        logging::Logger::getInstance().warn("Failed to send ACK");
    }
    
    // 요청 전송 완료 (응답은 백그라운드 스레드에서 받음)
    state_ = CommState::IDLE;
    logging::Logger::getInstance().debug("Payment approval request sent (async), waiting for response in background");
    return true;
}

} // namespace smartro
