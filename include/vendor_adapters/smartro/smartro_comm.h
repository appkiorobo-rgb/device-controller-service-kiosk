// include/vendor_adapters/smartro/smartro_comm.h
#pragma once

// Windows macro protection
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
#endif

#include "vendor_adapters/smartro/smartro_protocol.h"
#include "vendor_adapters/smartro/serial_port.h"
#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <chrono>

namespace smartro {

enum class CommState {
    IDLE,
    SENDING_REQUEST,
    WAITING_ACK,
    RECEIVING_RESPONSE,
    SENDING_ACK,
    COMPLETED,
    ERROR
};

// Response type (distinguished by Job Code)
enum class ResponseType {
    DEVICE_CHECK,           // 'a'
    PAYMENT_WAIT,           // 'e'
    CARD_UID_READ,         // 'f'
    RESET,                 // 'r'
    PAYMENT_APPROVAL,      // 'b'
    LAST_APPROVAL,         // 'l'
    SCREEN_SOUND_SETTING,  // 's'
    IC_CARD_CHECK,         // 'm'
    EVENT                  // '@'
};

// Unified response structure
// Note: Types DeviceCheckResponse, PaymentWaitResponse, etc. are defined in smartro_protocol.h
struct ResponseData {
    ResponseType type;
    char jobCode;
    std::vector<uint8_t> rawData;  // Original data
    
    // Type-specific data (use only the one matching the type)
    // Types are in the same namespace, so no namespace prefix needed
    DeviceCheckResponse deviceCheck;
    PaymentWaitResponse paymentWait;
    CardUidReadResponse cardUid;
    PaymentApprovalResponse paymentApproval;
    LastApprovalResponse lastApproval;
    ScreenSoundSettingResponse screenSound;
    IcCardCheckResponse icCard;
    EventResponse event;
};

class SmartroComm {
public:
    SmartroComm(SerialPort& serialPort);
    ~SmartroComm();
    
    // Start/stop response receiver thread
    void startResponseReceiver();
    void stopResponseReceiver();
    
    // Poll response (get asynchronous response)
    bool pollResponse(ResponseData& response, uint32_t timeoutMs = 0);
    
    // Legacy synchronous functions (for backward compatibility)
    // Send device check request and receive response
    bool sendDeviceCheckRequest(const std::string& terminalId,
                                DeviceCheckResponse& response,
                                uint32_t timeoutMs = 3000,
                                const std::string& preferredPort = "");
    
    // Send payment wait request and receive response
    bool sendPaymentWaitRequest(const std::string& terminalId, 
                                PaymentWaitResponse& response,
                                uint32_t timeoutMs = 3000);
    
    // Send card UID read request and receive response
    bool sendCardUidReadRequest(const std::string& terminalId, 
                                CardUidReadResponse& response,
                                uint32_t timeoutMs = 3000);
    
    // Wait for event (blocking, timeout available)
    // Events are automatically sent from device, so no ACK/NACK is sent
    bool waitForEvent(EventResponse& event, uint32_t timeoutMs = 0);  // timeoutMs=0 means infinite wait
    
    // Send terminal reset request and receive response
    bool sendResetRequest(const std::string& terminalId, uint32_t timeoutMs = 3000);
    
    // Send payment approval request (asynchronous - send request and return immediately)
    bool sendPaymentApprovalRequestAsync(const std::string& terminalId, 
                                        const PaymentApprovalRequest& request);
    
    // Send payment approval request and receive response (synchronous - for backward compatibility)
    bool sendPaymentApprovalRequest(const std::string& terminalId, 
                                    const PaymentApprovalRequest& request,
                                    PaymentApprovalResponse& response,
                                    uint32_t timeoutMs = 30000);  // Payment may take longer
    
    // Send transaction cancellation request and receive response
    bool sendTransactionCancelRequest(const std::string& terminalId,
                                     const TransactionCancelRequest& request,
                                     TransactionCancelResponse& response,
                                     uint32_t timeoutMs = 30000);
    
    // Send last approval response request and receive response
    bool sendLastApprovalResponseRequest(const std::string& terminalId, 
                                        LastApprovalResponse& response,
                                        uint32_t timeoutMs = 30000);
    
    // Send screen/sound setting request and receive response
    bool sendScreenSoundSettingRequest(const std::string& terminalId, 
                                       const ScreenSoundSettingRequest& request,
                                       ScreenSoundSettingResponse& response,
                                       uint32_t timeoutMs = 3000);
    
    // Send IC card check request and receive response
    bool sendIcCardCheckRequest(const std::string& terminalId, 
                               IcCardCheckResponse& response,
                               uint32_t timeoutMs = 3000);
    
    // Get state
    CommState getState() const;
    std::string getLastError() const;
    
private:
    SerialPort& serialPort_;
    CommState state_;
    std::string lastError_;
    mutable std::mutex commMutex_;  // Lock for thread safety
    
    // Asynchronous response reception
    std::atomic<bool> receiverRunning_;
    std::thread receiverThread_;
    std::queue<ResponseData> responseQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    static constexpr uint32_t ACK_TIMEOUT_MS = 5000;  // ACK wait timeout
    static constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000;  // Response receive timeout
    
    // Background response receiver thread
    void responseReceiverThread();
    
    // Parse response and add to queue
    void processResponse(const std::vector<uint8_t>& packet);
    
    // ACK/NACK handling
    bool waitForAck(uint32_t timeoutMs, std::vector<uint8_t>& responsePacket);
    bool sendAck();
    bool sendNack();
    
    // Receive response
    bool receiveResponse(std::vector<uint8_t>& responsePacket, uint32_t timeoutMs);
    
    // Read single byte (for ACK/NACK)
    bool readByte(uint8_t& byte, uint32_t timeoutMs);
    
    // Flush serial buffer
    void flushSerialBuffer();
    
    // Set error
    void setError(const std::string& error);
};

} // namespace smartro
