// include/vendor_adapters/smartro/smartro_protocol.h
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

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace smartro {

// Packet constants
constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;
constexpr uint8_t ACK = 0x06;
constexpr uint8_t NACK = 0x15;

// Header structure size
constexpr size_t HEADER_SIZE = 35;
constexpr size_t TAIL_SIZE = 2;
constexpr size_t MIN_PACKET_SIZE = HEADER_SIZE + TAIL_SIZE;  // When no data

// Job Codes
constexpr char JOB_CODE_DEVICE_CHECK = 'A';
constexpr char JOB_CODE_DEVICE_CHECK_RESPONSE = 'a';
constexpr char JOB_CODE_PAYMENT_WAIT = 'E';
constexpr char JOB_CODE_PAYMENT_WAIT_RESPONSE = 'e';
constexpr char JOB_CODE_CARD_UID_READ = 'F';
constexpr char JOB_CODE_CARD_UID_READ_RESPONSE = 'f';
constexpr char JOB_CODE_EVENT = '@';  // Event message
constexpr char JOB_CODE_RESET = 'R';
constexpr char JOB_CODE_RESET_RESPONSE = 'r';
constexpr char JOB_CODE_PAYMENT_APPROVAL = 'B';
constexpr char JOB_CODE_PAYMENT_APPROVAL_RESPONSE = 'b';
constexpr char JOB_CODE_TRANSACTION_CANCEL = 'C';
constexpr char JOB_CODE_TRANSACTION_CANCEL_RESPONSE = 'c';
constexpr char JOB_CODE_LAST_APPROVAL_RESPONSE = 'L';
constexpr char JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE = 'l';
constexpr char JOB_CODE_SCREEN_SOUND_SETTING = 'S';
constexpr char JOB_CODE_SCREEN_SOUND_SETTING_RESPONSE = 's';
constexpr char JOB_CODE_IC_CARD_CHECK = 'M';
constexpr char JOB_CODE_IC_CARD_CHECK_RESPONSE = 'm';

// Device check response structure
struct DeviceCheckResponse {
    char cardModuleStatus;      // N/O/X
    char rfModuleStatus;         // O/X
    char vanServerStatus;        // N/O/X/F
    char integrationServerStatus; // N/O/X/F
};

// Payment wait response structure (format not specified in docs)
struct PaymentWaitResponse {
    std::vector<uint8_t> data;  // Response data (format unspecified)
};

// Card UID read response structure (format not specified in docs)
struct CardUidReadResponse {
    std::vector<uint8_t> uid;  // Card UID (typically 4-8 bytes)
};

// Event type
enum class EventType {
    MS_CARD_DETECTED,      // @M - MS card detected
    RF_CARD_DETECTED,      // @R - RF card detected
    IC_CARD_DETECTED,      // @I - IC card detected
    IC_CARD_REMOVED,       // @O - IC card removed
    IC_CARD_FALLBACK,      // @F - IC card fallback
    UNKNOWN                // Unknown event
};

// Event response structure
struct EventResponse {
    EventType type;              // Event type
    std::vector<uint8_t> data;  // Event data (format not specified in docs)
};

// Payment approval request structure
struct PaymentApprovalRequest {
    uint8_t transactionType;    // 1: Approval, 2: Last transaction cancellation
    uint32_t amount;            // Approval amount (KRW)
    uint32_t tax;               // Tax (KRW)
    uint32_t service;           // Service charge (KRW)
    uint8_t installments;       // Installment months (00: lump sum)
    uint8_t signatureRequired;  // 1: No signature, 2: Signature required
};

// Payment approval response structure (157 bytes)
struct PaymentApprovalResponse {
    char transactionType;        // Transaction type: 1=Credit, 2=Cash Receipt, 3=Prepaid, 4=Zero Pay, 5=Kakao Mini, 6=Kakao Credit, X=Rejected
    char transactionMedium;      // Transaction medium: 1=IC, 2=MS, 3=RF, 4=QR, 5=KEYIN
    std::string cardNumber;      // Card number (20 bytes, masked)
    std::string approvalAmount;  // Approval/Cancellation amount (10 bytes)
    std::string tax;             // Tax (8 bytes)
    std::string serviceCharge;   // Service charge (8 bytes)
    std::string installments;    // Installment months (2 bytes)
    std::string approvalNumber;  // Approval number/Prepaid card info (12 bytes)
    std::string salesDate;       // Sales date (8 bytes, YYYYMMDD)
    std::string salesTime;       // Sales time (6 bytes, hhmmss)
    std::string transactionId;   // Transaction unique number (12 bytes)
    std::string merchantNumber; // Merchant number (15 bytes)
    std::string terminalNumber;  // Terminal number (14 bytes)
    std::string issuer;          // Issuer (20 bytes)
    std::string rejectionInfo;   // Rejection info (20 bytes, if rejected)
    std::string acquirer;        // Acquirer (20 bytes)
    
    // Original data (backup)
    std::vector<uint8_t> data;
    
    // Convenience functions
    bool isRejected() const { return transactionType == 'X' || transactionType == 'x'; }
    bool isSuccess() const { return !isRejected(); }
};

// Last approval response structure (same as PaymentApprovalResponse)
struct LastApprovalResponse {
    std::vector<uint8_t> data;  // Response data (157 bytes, same as PaymentApprovalResponse)
};

// Screen/sound setting request structure
struct ScreenSoundSettingRequest {
    uint8_t screenBrightness;   // 0-9
    uint8_t soundVolume;        // 0-9
    uint8_t touchSoundVolume;   // 0-9
};

// Screen/sound setting response structure
struct ScreenSoundSettingResponse {
    uint8_t screenBrightness;   // Set screen brightness
    uint8_t soundVolume;        // Set sound volume
    uint8_t touchSoundVolume;   // Set touch sound volume
};

// IC card check response structure
struct IcCardCheckResponse {
    char cardStatus;  // 'O': IC card inserted, 'X': No IC card
};

// Transaction cancellation request structure
struct TransactionCancelRequest {
    char cancelType;              // '1': Request message cancellation, '2': Last transaction cancellation
    uint8_t transactionType;      // Transaction type (same as PaymentApprovalRequest)
    uint32_t amount;              // Cancellation amount (KRW)
    uint32_t tax;                 // Tax (KRW)
    uint32_t service;             // Service charge (KRW)
    uint8_t installments;          // Installment months (00: lump sum)
    std::string approvalNumber;   // Approval number from original transaction (12 bytes)
    std::string originalDate;     // Original transaction date YYYYMMDD (8 bytes)
    std::string originalTime;     // Original transaction time hhmmss (6 bytes)
    std::string additionalInfo;    // Additional info (for PG cancellation, 30 digits)
};

// Transaction cancellation response structure (same as PaymentApprovalResponse)
struct TransactionCancelResponse {
    // Same structure as PaymentApprovalResponse
    char transactionType;        // Transaction type
    char transactionMedium;      // Transaction medium
    std::string cardNumber;      // Card number (20 bytes, masked)
    std::string approvalAmount;  // Approval/Cancellation amount (10 bytes)
    std::string tax;             // Tax (8 bytes)
    std::string serviceCharge;   // Service charge (8 bytes)
    std::string installments;    // Installment months (2 bytes)
    std::string approvalNumber;  // Approval number/Prepaid card info (12 bytes)
    std::string salesDate;       // Sales date (8 bytes, YYYYMMDD)
    std::string salesTime;       // Sales time (6 bytes, hhmmss)
    std::string transactionId;   // Transaction unique number (12 bytes)
    std::string merchantNumber; // Merchant number (15 bytes)
    std::string terminalNumber;  // Terminal number (14 bytes)
    std::string issuer;          // Issuer (20 bytes)
    std::string rejectionInfo;   // Rejection info (20 bytes, if rejected)
    std::string acquirer;        // Acquirer (20 bytes)
    
    // Original data (backup)
    std::vector<uint8_t> data;
    
    // Convenience functions
    bool isRejected() const { return transactionType == 'X' || transactionType == 'x'; }
    bool isSuccess() const { return !isRejected(); }
};

class SmartroProtocol {
public:
    // Create device check request packet
    static std::vector<uint8_t> createDeviceCheckRequest(const std::string& terminalId);
    
    // Create payment wait request packet
    static std::vector<uint8_t> createPaymentWaitRequest(const std::string& terminalId);
    
    // Create card UID read request packet
    static std::vector<uint8_t> createCardUidReadRequest(const std::string& terminalId);
    
    // Create reset request packet
    static std::vector<uint8_t> createResetRequest(const std::string& terminalId);
    
    // Create payment approval request packet
    static std::vector<uint8_t> createPaymentApprovalRequest(const std::string& terminalId,
                                                              const PaymentApprovalRequest& request);
    
    // Create transaction cancellation request packet
    static std::vector<uint8_t> createTransactionCancelRequest(const std::string& terminalId,
                                                                const TransactionCancelRequest& request);
    
    // Create last approval response request packet
    static std::vector<uint8_t> createLastApprovalResponseRequest(const std::string& terminalId);
    
    // Create screen/sound setting request packet
    static std::vector<uint8_t> createScreenSoundSettingRequest(const std::string& terminalId,
                                                                const ScreenSoundSettingRequest& request);
    
    // Create IC card check request packet
    static std::vector<uint8_t> createIcCardCheckRequest(const std::string& terminalId);
    
    // Parse packet
    static bool parsePacket(const uint8_t* data, size_t length, 
                           std::vector<uint8_t>& header, 
                           std::vector<uint8_t>& payload);
    
    // Calculate BCC (XOR from STX to ETX)
    static uint8_t calculateBCC(const uint8_t* data, size_t length);
    
    // Verify BCC
    static bool verifyBCC(const uint8_t* packet, size_t packetLength);
    
    // Parse device check response
    static bool parseDeviceCheckResponse(const uint8_t* data, size_t length, 
                                        DeviceCheckResponse& response);
    
    // Parse payment wait response
    static bool parsePaymentWaitResponse(const uint8_t* data, size_t length, 
                                         PaymentWaitResponse& response);
    
    // Parse card UID read response
    static bool parseCardUidReadResponse(const uint8_t* data, size_t length, 
                                         CardUidReadResponse& response);
    
    // Parse event response
    static bool parseEventResponse(const uint8_t* data, size_t length, 
                                  EventResponse& response);
    
    // Parse payment approval response
    static bool parsePaymentApprovalResponse(const uint8_t* data, size_t length, 
                                            PaymentApprovalResponse& response);
    
    // Parse transaction cancellation response (same as payment approval response)
    static bool parseTransactionCancelResponse(const uint8_t* data, size_t length, 
                                              TransactionCancelResponse& response);
    
    // Parse last approval response (same as payment approval response)
    static bool parseLastApprovalResponse(const uint8_t* data, size_t length, 
                                         LastApprovalResponse& response);
    
    // Parse screen/sound setting response
    static bool parseScreenSoundSettingResponse(const uint8_t* data, size_t length, 
                                               ScreenSoundSettingResponse& response);
    
    // Parse IC card check response
    static bool parseIcCardCheckResponse(const uint8_t* data, size_t length, 
                                        IcCardCheckResponse& response);
    
    // Generate DateTime (YYYYMMDDhhmmss)
    static std::string getCurrentDateTime();
    
    // Format Terminal ID (16 bytes, left-aligned, rest 0x00)
    static std::vector<uint8_t> formatTerminalId(const std::string& terminalId);
    
    // Convert USHORT to Little Endian
    static void writeUshortLE(uint16_t value, uint8_t* buffer);
    
    // Read Little Endian USHORT
    static uint16_t readUshortLE(const uint8_t* buffer);
    
    // Extract Data Length from packet
    static uint16_t extractDataLength(const uint8_t* header);
    
    // Extract Job Code
    static char extractJobCode(const uint8_t* header);
};

} // namespace smartro
