// src/vendor_adapters/smartro/smartro_protocol.cpp
#include "vendor_adapters/smartro/smartro_protocol.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cctype>
#include <iostream>

namespace device_controller::vendor::smartro {

SMARTROProtocol::SMARTROProtocol(std::shared_ptr<SerialPort> serialPort)
    : serialPort_(serialPort), terminalId_("KIOSK-01") {
    // Initialize device status
    // Flutter 구현 참고: Terminal ID는 'KIOSK-01' 사용
    lastDeviceStatus_ = {'X', 'X', 'X', 'X'};
}

SMARTROProtocol::~SMARTROProtocol() {
    shutdown();
}

bool SMARTROProtocol::initialize() {
    if (running_) {
        std::cout << "[PROTOCOL] Already initialized" << std::endl;
        return false;
    }

    std::cout << "[PROTOCOL] Starting receive thread..." << std::endl;
    running_ = true;
    receiveThread_ = std::thread(&SMARTROProtocol::receiveLoop, this);
    std::cout << "[PROTOCOL] Receive thread started" << std::endl;
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

    // 패킷 구조: STX(1) + HEADER(34) + DATA(N) + ETX(1) + BCC(1)
    // HEADER: Terminal ID(16) + DateTime(14) + Job Code(1) + Response Code(1) + Data Length(2)

    // STX (Start of Text) - 1 byte
    packet.push_back(STX); // 0x02

    // Terminal ID - 16 bytes (좌측 정렬, 나머지 0x00)
    std::string termId = padString(terminalId_, 16, '\0', false);
    for (size_t i = 0; i < 16; i++) {
        packet.push_back(termId[i]);
    }

    // DateTime - 14 bytes (YYYYMMDDhhmmss)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; i++) {
        packet.push_back(dateTime[i]);
    }

    // Job Code - 1 byte (CHAR)
    packet.push_back(static_cast<uint8_t>(jobCode));

    // Response Code - 1 byte (BYTE, 요청 시 0x00)
    packet.push_back(0x00);

    // Data Length - 2 bytes (USHORT, Little Endian)
    uint16_t dataLength = static_cast<uint16_t>(data.size());
    packet.push_back(static_cast<uint8_t>(dataLength & 0xFF));        // Low byte
    packet.push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));  // High byte

    // Data - N bytes (가변 길이, 장치체크 요청 시 0 bytes)
    packet.insert(packet.end(), data.begin(), data.end());

    // ETX (End of Text) - 1 byte
    packet.push_back(ETX); // 0x03

    // BCC (Block Check Character) - 1 byte (STX부터 ETX까지 XOR)
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
    // ACK는 receiveLoop에서 받아서 ackReceived_ 플래그를 설정함
    // 여기서는 그 플래그를 확인하고 기다림
    std::unique_lock<std::mutex> lock(ackMutex_);
    
    // 이미 ACK를 받았는지 먼저 확인
    if (ackReceived_.load()) {
        ackReceived_ = false; // Reset for next use
        return true;
    }
    
    // ACK를 기다림
    ackReceived_ = false; // Reset before waiting
    auto timeout = std::chrono::milliseconds(timeoutMs);
    bool received = ackCondition_.wait_for(lock, timeout, [this] { 
        return ackReceived_.load(); 
    });
    
    if (received) {
        ackReceived_ = false; // Reset for next use
        return true;
    }
    
    return false;
}

bool SMARTROProtocol::sendDeviceCheck() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 장치체크 요청: Data 없음 (Data Length = 0)
    // 패킷 구조: STX(1) + HEADER(35) + ETX(1) + BCC(1) = 총 38 bytes
    // HEADER: Terminal ID(16) + DateTime(14) + Job Code('A') + Response Code(0x00) + Data Length(0x00 0x00)
    auto buildStartTime = std::chrono::steady_clock::now();
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Building device check packet (Job Code 'A')..." << std::endl;
    std::vector<uint8_t> data; // Empty data for device check (Data Length = 0)
    std::vector<uint8_t> packet = buildPacket(JOB_CODE_DEVICE_CHECK, data);
    auto buildElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - buildStartTime).count();
    
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Packet size: " << packet.size() << " bytes (build took " << buildElapsed << "ms)" << std::endl;
    
    auto sendStartTime = std::chrono::steady_clock::now();
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Sending packet..." << std::endl;
    if (!sendPacket(packet)) {
        std::cerr << "[" << getTimestamp() << "] [PROTOCOL] ERROR: Failed to send packet" << std::endl;
        return false;
    }
    auto sendElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - sendStartTime).count();
    
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Packet sent (" << sendElapsed << "ms), waiting for ACK..." << std::endl;
    waitingForResponse_ = true;
    
    // Wait for ACK (0x06) from terminal
    // 프로토콜: 요청 전문 전송 후 수신 정상 시 ACK (0x06) 수신
    // 디바이스 감지 시 빠른 실패를 위해 타임아웃을 500ms로 단축
    // 실제 SMARTRO 장치는 빠르게 응답하므로 짧은 타임아웃으로 충분
    auto ackStartTime = std::chrono::steady_clock::now();
    if (!waitForACK(500)) {
        auto ackElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ackStartTime).count();
        std::cerr << "[" << getTimestamp() << "] [PROTOCOL] ERROR: ACK timeout (" << ackElapsed << "ms) - device not responding" << std::endl;
        waitingForResponse_ = false;
        return false;
    }
    auto ackElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - ackStartTime).count();
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] ACK received (" << ackElapsed << "ms), waiting for response..." << std::endl;

    // Response will be handled by receiveLoop
    // 응답 전문: Job Code 'a' + Data(4 bytes: 카드모듈, RF모듈, VAN서버, 연동서버 상태)
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
    // 장치체크 응답: Data Length = 4 bytes
    // [0]: 카드 모듈 상태 (N/O/X)
    // [1]: RF 모듈 상태 (O/X)
    // [2]: VAN 서버 연결 상태 (N/O/X/F)
    // [3]: 연동 서버 연결 상태 (N/O/X/F)
    if (data.size() < 4) {
        std::cerr << "[PROTOCOL] ERROR: Device check response too short: " << data.size() << " bytes (expected 4)" << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    lastDeviceStatus_.cardModuleStatus = static_cast<char>(data[0]);
    lastDeviceStatus_.rfModuleStatus = static_cast<char>(data[1]);
    lastDeviceStatus_.vanServerStatus = static_cast<char>(data[2]);
    lastDeviceStatus_.integrationServerStatus = static_cast<char>(data[3]);
    
    std::cout << "[PROTOCOL] Device check response parsed - Card:" << lastDeviceStatus_.cardModuleStatus 
              << " RF:" << lastDeviceStatus_.rfModuleStatus 
              << " VAN:" << lastDeviceStatus_.vanServerStatus 
              << " Integration:" << lastDeviceStatus_.integrationServerStatus << std::endl;

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
    // Flutter 구현 참고: STX 동기화
    // Find STX
    size_t stxPos = offset;
    while (stxPos < buffer.size() && buffer[stxPos] != STX) {
        stxPos++;
    }

    if (stxPos >= buffer.size()) {
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] No STX found, offset=" << offset << ", buffer_size=" << buffer.size() << std::endl;
        return false;
    }

    if (stxPos > offset) {
        // STX 전 데이터 폐기
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] Skipping " << (stxPos - offset) << " bytes before STX" << std::endl;
        offset = stxPos;
    }

    size_t startOffset = offset;
    offset++; // Skip STX

    // 고정 헤더 길이 확인: STX(1) + TerminalID(16) + DateTime(14) + JobCode(1) + Response(1) + Length(2) = 35
    const size_t needFixed = 35;
    if (buffer.size() - offset < needFixed) {
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] Not enough data for header, need=" << needFixed 
                  << ", available=" << (buffer.size() - offset) << std::endl;
        return false;
    }

    // Terminal ID (16 bytes) - skip
    offset += 16;
    
    // DateTime (14 bytes) - skip
    offset += 14;
    
    // Job Code (1 byte) - trim nulls and spaces (Flutter 구현 참고)
    char jobCodeRaw = static_cast<char>(buffer[offset]);
    // Trim nulls and spaces (right trim)
    char jobCode = jobCodeRaw;
    if (jobCodeRaw == 0x00 || jobCodeRaw == 0x20) {
        jobCode = ' '; // Will be handled as empty
    }
    offset++;
    
    // Response Code (1 byte) - skip
    offset++;
    
    // Data Length (2 bytes, little endian)
    uint16_t dataLength = static_cast<uint16_t>(buffer[offset]) | 
                         (static_cast<uint16_t>(buffer[offset + 1]) << 8);
    offset += 2;

    // 총 프레임 길이: STX(1) + Header(35) + Body(N) + ETX(1) + BCC(1)
    size_t totalLen = 1 + needFixed + dataLength + 2; // +2 for ETX and BCC
    if (buffer.size() - startOffset < totalLen) {
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] Not enough data for complete packet, need=" << totalLen 
                  << ", available=" << (buffer.size() - startOffset) << ", dataLength=" << dataLength << std::endl;
        return false; // Need more data
    }

    // Read data
    std::vector<uint8_t> data(buffer.begin() + offset, buffer.begin() + offset + dataLength);
    offset += dataLength;

    // Check ETX
    if (buffer[offset++] != ETX) {
        std::cerr << "[PROTOCOL] ERROR: Missing ETX" << std::endl;
        offset = startOffset + 1; // Skip this STX and try again
        return false;
    }

    // Verify BCC (STX 포함 XOR, Flutter 구현 참고)
    uint8_t expectedBCC = calculateBCC(buffer, startOffset, offset);
    uint8_t receivedBCC = buffer[offset++];
    
    if (expectedBCC != receivedBCC) {
        std::cerr << "[PROTOCOL] ERROR: BCC mismatch (expected=0x" << std::hex << (int)expectedBCC 
                  << ", received=0x" << (int)receivedBCC << std::dec << ")" << std::endl;
        // BCC mismatch - send NACK
        std::vector<uint8_t> nack = {NACK};
        serialPort_->write(nack);
        offset = startOffset + 1; // Skip this STX and try again
        return false;
    }

    // Send ACK (except for event packets)
    // Flutter: 이벤트 전문은 ACK/NACK 미전송
    if (jobCode != JOB_CODE_EVENT && jobCode != ' ') {
        std::vector<uint8_t> ack = {ACK};
        serialPort_->write(ack);
    }

    // Parse response based on job code (case-insensitive, Flutter 구현 참고)
    // Flutter: jobCode.trimRight().toLowerCase()
    bool parsed = false;
    
    // Trim nulls and spaces from jobCode
    char jobCodeClean = jobCode;
    if (jobCode == 0x00 || jobCode == 0x20) {
        jobCodeClean = ' '; // Will be ignored
    }
    
    char jobCodeLower = std::tolower(jobCodeClean);
    
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Parsing packet - JobCode: '" << jobCodeLower << "' (raw: 0x" 
              << std::hex << (int)jobCodeRaw << std::dec << "), DataLength: " << dataLength 
              << ", TotalLen: " << totalLen << ", BufferSize: " << buffer.size() << std::endl;
    
    if (jobCodeLower == 'a') { // JOB_CODE_RESP_DEVICE_CHECK
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] Processing device check response (JobCode 'a')" << std::endl;
        parsed = parseDeviceCheckResponse(data);
        if (parsed) {
            std::cout << "[" << getTimestamp() << "] [PROTOCOL] Device check response parsed successfully" << std::endl;
        } else {
            std::cout << "[" << getTimestamp() << "] [PROTOCOL] Failed to parse device check response" << std::endl;
        }
    } else if (jobCodeLower == 'b') { // JOB_CODE_RESP_PAYMENT
        parsed = parsePaymentResponse(data);
    } else if (jobCode == JOB_CODE_EVENT) {
        parsed = parseEventPacket(data);
    } else if (jobCodeLower == 'c' || jobCodeLower == 'r') { // Cancel/Reset
        // Cancel/Reset responses - just mark as parsed
        parsed = true;
    } else {
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] Unknown JobCode: '" << jobCodeLower << "'" << std::endl;
    }

    if (parsed) {
        std::cout << "[" << getTimestamp() << "] [PROTOCOL] Packet parsed, setting waitingForResponse_=false" << std::endl;
        waitingForResponse_ = false;
    }

    return parsed;
}

void SMARTROProtocol::receiveLoop() {
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Receive loop started" << std::endl;
    std::vector<uint8_t> receiveBuffer;
    size_t parseOffset = 0;

    while (running_) {
        std::vector<uint8_t> buffer;
        int bytesRead = serialPort_->read(buffer, 4096, 100);

        if (bytesRead > 0) {
            std::cout << "[" << getTimestamp() << "] [PROTOCOL] Received " << bytesRead << " bytes: ";
            for (int i = 0; i < bytesRead && i < 20; i++) { // 최대 20바이트만 출력
                std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)buffer[i] << " ";
            }
            if (bytesRead > 20) std::cout << "...";
            std::cout << std::dec << std::endl;
            
            receiveBuffer.insert(receiveBuffer.end(), buffer.begin(), buffer.end());
            
            std::cout << "[" << getTimestamp() << "] [PROTOCOL] Buffer size after insert: " << receiveBuffer.size() << " bytes" << std::endl;

            // Flutter 구현 참고: ACK(0x06) 먼저 처리
            // 버퍼의 시작부터 ACK를 찾아야 함 (parseOffset과 무관하게)
            int ackCount = 0;
            size_t ackOffset = 0;
            while (ackOffset < receiveBuffer.size() && receiveBuffer[ackOffset] == ACK) {
                ackCount++;
                ackOffset++;
                
                // ACK를 받았음을 알림 (waitForACK가 기다리고 있을 수 있음)
                {
                    std::lock_guard<std::mutex> lock(ackMutex_);
                    ackReceived_ = true;
                }
                ackCondition_.notify_one();
            }
            
            // ACK를 찾았으면 버퍼에서 제거하고 parseOffset 조정
            if (ackCount > 0) {
                std::cout << "[" << getTimestamp() << "] [PROTOCOL] Received " << ackCount << " ACK(s), removing from buffer" << std::endl;
                receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + ackCount);
                parseOffset = 0; // ACK 제거 후 parseOffset 리셋
                std::cout << "[" << getTimestamp() << "] [PROTOCOL] Buffer size after ACK removal: " << receiveBuffer.size() << " bytes" << std::endl;
            }

            // Try to parse packets
            while (parseOffset < receiveBuffer.size()) {
                size_t oldOffset = parseOffset;
                std::cout << "[" << getTimestamp() << "] [PROTOCOL] Attempting to parse packet, offset=" << parseOffset << ", buffer_size=" << receiveBuffer.size() << std::endl;
                if (parsePacket(receiveBuffer, parseOffset)) {
                    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Packet parsed successfully, parsed_bytes=" << parseOffset << std::endl;
                    // Packet parsed successfully, remove processed data
                    receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + parseOffset);
                    parseOffset = 0;
                    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Buffer size after packet removal: " << receiveBuffer.size() << " bytes" << std::endl;
                } else if (parseOffset == oldOffset) {
                    // No progress, break and wait for more data
                    std::cout << "[" << getTimestamp() << "] [PROTOCOL] No progress in parsing, waiting for more data" << std::endl;
                    break;
                } else {
                    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Parse offset advanced but packet not complete, offset=" << parseOffset << std::endl;
                }
            }

            // Keep some data for next iteration (in case packet is split)
            // Flutter: 버퍼가 너무 커지는 걸 방지, 마지막 몇 바이트만 유지
            if (receiveBuffer.size() > 4096) {
                receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + (receiveBuffer.size() - 4096));
                parseOffset = 0;
            }
        }
    }
    
    std::cout << "[" << getTimestamp() << "] [PROTOCOL] Receive loop stopped" << std::endl;
}

} // namespace device_controller::vendor::smartro
