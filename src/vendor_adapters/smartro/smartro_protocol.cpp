// src/vendor_adapters/smartro/smartro_protocol.cpp
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>

namespace device_controller::vendor::smartro {

SMARTROProtocol::SMARTROProtocol(std::shared_ptr<SerialPort> serialPort)
    : serialPort_(serialPort), terminalId_("TERMINAL001") {
    // Initialize device status
    lastDeviceStatus_ = {'X', 'X', 'X', 'X'};
}

SMARTROProtocol::~SMARTROProtocol() {
    shutdown();
}

bool SMARTROProtocol::initialize() {
    if (running_) {
        return false;
    }

    running_ = true;
    receiveThread_ = std::thread(&SMARTROProtocol::receiveLoop, this);
    return true;
}

void SMARTROProtocol::shutdown() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

std::string SMARTROProtocol::getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);

    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_buf.tm_year + 1900)
        << std::setw(2) << (tm_buf.tm_mon + 1)
        << std::setw(2) << tm_buf.tm_mday
        << std::setw(2) << tm_buf.tm_hour
        << std::setw(2) << tm_buf.tm_min
        << std::setw(2) << tm_buf.tm_sec;

    return oss.str();
}

std::string SMARTROProtocol::padString(const std::string& str, size_t length, char padChar, bool rightAlign) {
    if (str.length() >= length) {
        return str.substr(0, length);
    }

    std::string padded = str;
    if (rightAlign) {
        padded.insert(0, length - str.length(), padChar);
    } else {
        padded.append(length - str.length(), padChar);
    }
    return padded;
}

int64_t SMARTROProtocol::parseAmount(const std::string& str) {
    try {
        return std::stoll(str);
    } catch (...) {
        return 0;
    }
}

std::string SMARTROProtocol::formatAmount(int64_t amount, size_t length) {
    return padString(std::to_string(amount), length, '0', true);
}

uint8_t SMARTROProtocol::calculateBCC(const std::vector<uint8_t>& packet, size_t start, size_t end) {
    uint8_t bcc = 0;
    for (size_t i = start; i < end && i < packet.size(); i++) {
        bcc ^= packet[i];
    }
    return bcc;
}

std::vector<uint8_t> SMARTROProtocol::buildPacket(char jobCode, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet;

    // STX
    packet.push_back(STX);

    // Terminal ID (16 bytes, left-aligned, rest filled with 0x00)
    std::string termId = padString(terminalId_, 16, '\0', false);
    for (size_t i = 0; i < 16; i++) {
        packet.push_back(termId[i]);
    }

    // DateTime (14 bytes, YYYYMMDDhhmmss)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; i++) {
        packet.push_back(dateTime[i]);
    }

    // Job Code (1 byte)
    packet.push_back(static_cast<uint8_t>(jobCode));

    // Response Code (1 byte, 0x00 for request)
    packet.push_back(0x00);

    // Data Length (2 bytes, little endian)
    uint16_t dataLength = static_cast<uint16_t>(data.size());
    packet.push_back(static_cast<uint8_t>(dataLength & 0xFF));
    packet.push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));

    // Data
    packet.insert(packet.end(), data.begin(), data.end());

    // ETX
    packet.push_back(ETX);

    // BCC (XOR from STX to ETX)
    uint8_t bcc = calculateBCC(packet, 0, packet.size());
    packet.push_back(bcc);

    return packet;
}

bool SMARTROProtocol::sendPacket(const std::vector<uint8_t>& packet) {
    if (!serialPort_ || !serialPort_->isOpen()) {
        return false;
    }

    int written = serialPort_->write(packet);
    return written == static_cast<int>(packet.size());
}

bool SMARTROProtocol::waitForACK(DWORD timeoutMs) {
    std::vector<uint8_t> buffer;
    int bytesRead = serialPort_->read(buffer, 1, timeoutMs);
    
    if (bytesRead == 1 && buffer[0] == ACK) {
        return true;
    }
    
    return false;
}

bool SMARTROProtocol::sendDeviceCheck() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<uint8_t> data; // Empty data for device check
    std::vector<uint8_t> packet = buildPacket(JOB_CODE_DEVICE_CHECK, data);
    
    if (!sendPacket(packet)) {
        return false;
    }

    waitingForResponse_ = true;
    
    // Wait for ACK
    if (!waitForACK(3000)) {
        waitingForResponse_ = false;
        return false;
    }

    // Response will be handled by receiveLoop
    return true;
}

bool SMARTROProtocol::sendPaymentRequest(int64_t amount, int64_t tax, int64_t serviceFee,
                                         int installmentMonths, bool requiresSignature) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint8_t> data;
    
    // 거래구분코드 (1 byte): 1 승인 / 2 마지막 거래 취소
    data.push_back('1');
    
    // 승인금액 (10 bytes, right-aligned, zero-padded)
    std::string amountStr = formatAmount(amount, 10);
    for (char c : amountStr) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    // 세금 (8 bytes)
    std::string taxStr = formatAmount(tax, 8);
    for (char c : taxStr) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    // 봉사료 (8 bytes)
    std::string serviceStr = formatAmount(serviceFee, 8);
    for (char c : serviceStr) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    // 할부개월 (2 bytes): 00 일시불
    std::string installmentStr = padString(std::to_string(installmentMonths), 2, '0', true);
    for (char c : installmentStr) {
        data.push_back(static_cast<uint8_t>(c));
    }
    
    // 서명여부 (1 byte): 1 비서명 / 2 서명
    data.push_back(requiresSignature ? '2' : '1');

    std::vector<uint8_t> packet = buildPacket(JOB_CODE_PAYMENT, data);
    
    if (!sendPacket(packet)) {
        return false;
    }

    waitingForResponse_ = true;
    
    // Wait for ACK
    if (!waitForACK(3000)) {
        waitingForResponse_ = false;
        return false;
    }

    return true;
}

bool SMARTROProtocol::sendCancelRequest(bool isLastTransactionCancel) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint8_t> data;
    // 취소구분코드 (1 byte): 1 요청 취소 / 2 마지막 거래 취소
    data.push_back(isLastTransactionCancel ? '2' : '1');

    std::vector<uint8_t> packet = buildPacket(JOB_CODE_CANCEL, data);
    
    if (!sendPacket(packet)) {
        return false;
    }

    waitingForResponse_ = true;
    
    // Wait for ACK
    if (!waitForACK(3000)) {
        waitingForResponse_ = false;
        return false;
    }

    return true;
}

bool SMARTROProtocol::sendResetRequest() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint8_t> data; // Empty data for reset
    std::vector<uint8_t> packet = buildPacket(JOB_CODE_RESET, data);
    
    if (!sendPacket(packet)) {
        return false;
    }

    waitingForResponse_ = true;
    
    // Wait for ACK
    if (!waitForACK(3000)) {
        waitingForResponse_ = false;
        return false;
    }

    return true;
}

void SMARTROProtocol::setEventCallback(ProtocolEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = callback;
}

DeviceStatus SMARTROProtocol::getLastDeviceStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastDeviceStatus_;
}

PaymentResponse SMARTROProtocol::getLastPaymentResponse() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastPaymentResponse_;
}

bool SMARTROProtocol::isWaitingForResponse() const {
    return waitingForResponse_;
}

bool SMARTROProtocol::parseDeviceCheckResponse(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    lastDeviceStatus_.cardModuleStatus = static_cast<char>(data[0]);
    lastDeviceStatus_.rfModuleStatus = static_cast<char>(data[1]);
    lastDeviceStatus_.vanServerStatus = static_cast<char>(data[2]);
    lastDeviceStatus_.integrationServerStatus = static_cast<char>(data[3]);

    return true;
}

bool SMARTROProtocol::parsePaymentResponse(const std::vector<uint8_t>& data) {
    // Payment response is 157 bytes according to spec
    if (data.size() < 157) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t offset = 0;
    
    // 거래구분코드 (1)
    lastPaymentResponse_.transactionType = std::string(1, static_cast<char>(data[offset++]));
    
    // 거래매체 (1)
    lastPaymentResponse_.paymentMedia = std::string(1, static_cast<char>(data[offset++]));
    
    // 카드번호 (마스킹) (16)
    lastPaymentResponse_.cardNumber = std::string(reinterpret_cast<const char*>(data.data() + offset), 16);
    offset += 16;
    
    // 승인금액 (10)
    std::string amountStr(reinterpret_cast<const char*>(data.data() + offset), 10);
    lastPaymentResponse_.amount = parseAmount(amountStr);
    offset += 10;
    
    // 세금 (8)
    std::string taxStr(reinterpret_cast<const char*>(data.data() + offset), 8);
    lastPaymentResponse_.tax = parseAmount(taxStr);
    offset += 8;
    
    // 봉사료 (8)
    std::string serviceStr(reinterpret_cast<const char*>(data.data() + offset), 8);
    lastPaymentResponse_.serviceFee = parseAmount(serviceStr);
    offset += 8;
    
    // 할부 (2)
    std::string installmentStr(reinterpret_cast<const char*>(data.data() + offset), 2);
    lastPaymentResponse_.installmentMonths = static_cast<int>(parseAmount(installmentStr));
    offset += 2;
    
    // 승인번호 (12)
    lastPaymentResponse_.approvalNumber = std::string(reinterpret_cast<const char*>(data.data() + offset), 12);
    offset += 12;
    
    // 매출일자 (8)
    lastPaymentResponse_.saleDate = std::string(reinterpret_cast<const char*>(data.data() + offset), 8);
    offset += 8;
    
    // 매출시간 (6)
    lastPaymentResponse_.saleTime = std::string(reinterpret_cast<const char*>(data.data() + offset), 6);
    offset += 6;
    
    // 거래고유번호 (12)
    lastPaymentResponse_.transactionId = std::string(reinterpret_cast<const char*>(data.data() + offset), 12);
    offset += 12;
    
    // 가맹점번호 (10)
    lastPaymentResponse_.merchantNumber = std::string(reinterpret_cast<const char*>(data.data() + offset), 10);
    offset += 10;
    
    // 단말기번호 (8)
    lastPaymentResponse_.terminalNumber = std::string(reinterpret_cast<const char*>(data.data() + offset), 8);
    offset += 8;
    
    // 발급사 코드 (2)
    lastPaymentResponse_.issuerCode = std::string(reinterpret_cast<const char*>(data.data() + offset), 2);
    offset += 2;
    
    // 매입사 코드 (2)
    lastPaymentResponse_.acquirerCode = std::string(reinterpret_cast<const char*>(data.data() + offset), 2);
    offset += 2;
    
    // 발급사 메시지 (20)
    lastPaymentResponse_.issuerMessage = std::string(reinterpret_cast<const char*>(data.data() + offset), 20);
    offset += 20;
    
    // 매입사 메시지 (20)
    lastPaymentResponse_.acquirerMessage = std::string(reinterpret_cast<const char*>(data.data() + offset), 20);
    offset += 20;
    
    // VAN 응답 코드 (2)
    lastPaymentResponse_.vanResponseCode = std::string(reinterpret_cast<const char*>(data.data() + offset), 2);
    offset += 2;

    return true;
}

bool SMARTROProtocol::parseEventPacket(const std::vector<uint8_t>& data) {
    if (data.size() < 2) {
        return false;
    }

    char eventCode = static_cast<char>(data[1]);
    std::string eventData;
    if (data.size() > 2) {
        eventData = std::string(reinterpret_cast<const char*>(data.data() + 2), data.size() - 2);
    }

    if (eventCallback_) {
        eventCallback_(eventCode, eventData);
    }

    return true;
}

bool SMARTROProtocol::parsePacket(const std::vector<uint8_t>& buffer, size_t& offset) {
    // Find STX
    while (offset < buffer.size() && buffer[offset] != STX) {
        offset++;
    }

    if (offset >= buffer.size()) {
        return false;
    }

    size_t startOffset = offset;
    offset++; // Skip STX

    // Need at least header (35 bytes) + ETX + BCC
    if (buffer.size() - offset < 37) {
        return false;
    }

    // Read header
    size_t headerStart = offset;
    
    // Terminal ID (16 bytes) - skip
    offset += 16;
    
    // DateTime (14 bytes) - skip
    offset += 14;
    
    // Job Code (1 byte)
    char jobCode = static_cast<char>(buffer[offset++]);
    
    // Response Code (1 byte) - skip
    offset++;
    
    // Data Length (2 bytes, little endian)
    uint16_t dataLength = static_cast<uint16_t>(buffer[offset]) | 
                         (static_cast<uint16_t>(buffer[offset + 1]) << 8);
    offset += 2;

    // Check if we have enough data
    if (buffer.size() - offset < dataLength + 2) { // +2 for ETX and BCC
        offset = startOffset; // Reset to try again later
        return false;
    }

    // Read data
    std::vector<uint8_t> data(buffer.begin() + offset, buffer.begin() + offset + dataLength);
    offset += dataLength;

    // Check ETX
    if (buffer[offset++] != ETX) {
        offset = startOffset + 1; // Skip this STX and try again
        return false;
    }

    // Verify BCC
    uint8_t expectedBCC = calculateBCC(buffer, startOffset, offset);
    uint8_t receivedBCC = buffer[offset++];
    
    if (expectedBCC != receivedBCC) {
        // BCC mismatch - send NACK
        std::vector<uint8_t> nack = {NACK};
        serialPort_->write(nack);
        offset = startOffset + 1; // Skip this STX and try again
        return false;
    }

    // Send ACK (except for event packets)
    if (jobCode != JOB_CODE_EVENT) {
        std::vector<uint8_t> ack = {ACK};
        serialPort_->write(ack);
    }

    // Parse response based on job code
    bool parsed = false;
    if (jobCode == JOB_CODE_RESP_DEVICE_CHECK) {
        parsed = parseDeviceCheckResponse(data);
    } else if (jobCode == JOB_CODE_RESP_PAYMENT) {
        parsed = parsePaymentResponse(data);
    } else if (jobCode == JOB_CODE_EVENT) {
        parsed = parseEventPacket(data);
    } else if (jobCode == JOB_CODE_RESP_CANCEL || jobCode == JOB_CODE_RESP_RESET) {
        // Cancel/Reset responses - just mark as parsed
        parsed = true;
    }

    if (parsed) {
        waitingForResponse_ = false;
    }

    return parsed;
}

void SMARTROProtocol::receiveLoop() {
    std::vector<uint8_t> receiveBuffer;
    size_t parseOffset = 0;

    while (running_) {
        std::vector<uint8_t> buffer;
        int bytesRead = serialPort_->read(buffer, 4096, 100);

        if (bytesRead > 0) {
            receiveBuffer.insert(receiveBuffer.end(), buffer.begin(), buffer.end());

            // Try to parse packets
            while (parseOffset < receiveBuffer.size()) {
                size_t oldOffset = parseOffset;
                if (parsePacket(receiveBuffer, parseOffset)) {
                    // Packet parsed successfully, remove processed data
                    receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + parseOffset);
                    parseOffset = 0;
                } else if (parseOffset == oldOffset) {
                    // No progress, break and wait for more data
                    break;
                }
            }

            // Keep some data for next iteration (in case packet is split)
            if (receiveBuffer.size() > 4096) {
                receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + (receiveBuffer.size() - 4096));
                parseOffset = 0;
            }
        }
    }
}

} // namespace device_controller::vendor::smartro
