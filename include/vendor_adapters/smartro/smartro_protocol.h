// include/vendor_adapters/smartro/smartro_protocol.h
#pragma once

#include "vendor_adapters/smartro/serial_port.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <iomanip>
#include <sstream>

// Helper function to get current timestamp string
inline std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    localtime_s(&tm_buf, &time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

namespace device_controller::vendor::smartro {

// SMARTRO protocol constants
constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;
constexpr uint8_t ACK = 0x06;
constexpr uint8_t NACK = 0x15;

// Job codes (request)
constexpr char JOB_CODE_DEVICE_CHECK = 'A';
constexpr char JOB_CODE_PAYMENT = 'B';
constexpr char JOB_CODE_CANCEL = 'C';
constexpr char JOB_CODE_RESET = 'R';

// Job codes (response)
constexpr char JOB_CODE_RESP_DEVICE_CHECK = 'a';
constexpr char JOB_CODE_RESP_PAYMENT = 'b';
constexpr char JOB_CODE_RESP_CANCEL = 'c';
constexpr char JOB_CODE_RESP_RESET = 'r';
constexpr char JOB_CODE_EVENT = '@';

// Event codes
constexpr char EVENT_MS_CARD = 'M';   // MS 카드 인식
constexpr char EVENT_RF_CARD = 'R';   // RF 카드 인식
constexpr char EVENT_IC_CARD = 'I';   // IC 카드 인식
constexpr char EVENT_CARD_REMOVED = 'O'; // IC 카드 제거
constexpr char EVENT_FALLBACK = 'F';   // IC 카드 Fallback

// Device check response status
struct DeviceStatus {
    char cardModuleStatus;      // N/O/X
    char rfModuleStatus;         // O/X
    char vanServerStatus;        // N/O/X/F
    char integrationServerStatus; // N/O/X/F
};

// Payment response data
struct PaymentResponse {
    std::string transactionType;      // 거래구분코드
    std::string paymentMedia;         // 거래매체 (IC/MS/RF/QR)
    std::string cardNumber;           // 카드번호 (마스킹)
    int64_t amount;                   // 승인금액
    int64_t tax;                      // 세금
    int64_t serviceFee;               // 봉사료
    int installmentMonths;            // 할부개월
    std::string approvalNumber;       // 승인번호
    std::string saleDate;             // 매출일자
    std::string saleTime;             // 매출시간
    std::string transactionId;        // 거래고유번호
    std::string merchantNumber;       // 가맹점번호
    std::string terminalNumber;       // 단말기번호
    std::string issuerCode;           // 발급사 코드
    std::string acquirerCode;         // 매입사 코드
    std::string issuerMessage;        // 발급사 메시지
    std::string acquirerMessage;      // 매입사 메시지
    std::string vanResponseCode;      // VAN 응답 코드
};

// Event callback type
using ProtocolEventCallback = std::function<void(char eventCode, const std::string& data)>;

// SMARTROProtocol - handles SMARTRO protocol communication
class SMARTROProtocol {
public:
    SMARTROProtocol(std::shared_ptr<SerialPort> serialPort);
    ~SMARTROProtocol();

    // Initialize protocol (start receive thread)
    bool initialize();

    // Shutdown protocol
    void shutdown();

    // Send device check request (Job Code A)
    // Returns true if request sent successfully
    bool sendDeviceCheck();

    // Send payment request (Job Code B)
    // amount: amount in won (원)
    // tax: VAT in won
    // serviceFee: service fee in won
    // installmentMonths: 00 for lump sum
    // requiresSignature: true if signature required
    bool sendPaymentRequest(int64_t amount, int64_t tax, int64_t serviceFee, 
                           int installmentMonths, bool requiresSignature);

    // Send cancel request (Job Code C)
    // isLastTransactionCancel: true for last transaction cancel
    bool sendCancelRequest(bool isLastTransactionCancel);

    // Send reset request (Job Code R)
    bool sendResetRequest();

    // Set event callback
    void setEventCallback(ProtocolEventCallback callback);

    // Get last device status
    DeviceStatus getLastDeviceStatus() const;

    // Get last payment response
    PaymentResponse getLastPaymentResponse() const;

    // Check if waiting for response
    bool isWaitingForResponse() const;

private:
    std::shared_ptr<SerialPort> serialPort_;
    std::atomic<bool> running_{false};
    std::thread receiveThread_;
    mutable std::mutex mutex_;
    
    ProtocolEventCallback eventCallback_;
    DeviceStatus lastDeviceStatus_;
    PaymentResponse lastPaymentResponse_;
    std::atomic<bool> waitingForResponse_{false};
    std::atomic<bool> ackReceived_{false};
    std::mutex ackMutex_;
    std::condition_variable ackCondition_;
    std::string terminalId_;
    
    // Packet building
    std::vector<uint8_t> buildPacket(char jobCode, const std::vector<uint8_t>& data);
    uint8_t calculateBCC(const std::vector<uint8_t>& packet, size_t start, size_t end);
    
    // Packet parsing
    bool parsePacket(const std::vector<uint8_t>& buffer, size_t& offset);
    bool parseDeviceCheckResponse(const std::vector<uint8_t>& data);
    bool parsePaymentResponse(const std::vector<uint8_t>& data);
    bool parseEventPacket(const std::vector<uint8_t>& data);
    
    // Communication
    bool sendPacket(const std::vector<uint8_t>& packet);
    bool waitForACK(DWORD timeoutMs = 3000);
    void receiveLoop();
    
    // Helper functions
    std::string getCurrentDateTime();
    std::string padString(const std::string& str, size_t length, char padChar = '0', bool rightAlign = true);
    int64_t parseAmount(const std::string& str);
    std::string formatAmount(int64_t amount, size_t length);
};

} // namespace device_controller::vendor::smartro
