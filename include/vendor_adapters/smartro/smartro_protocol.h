// include/vendor_adapters/smartro/smartro_protocol.h
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace smartro {

// 패킷 상수
constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;
constexpr uint8_t ACK = 0x06;
constexpr uint8_t NACK = 0x15;

// Header 구조 크기
constexpr size_t HEADER_SIZE = 35;
constexpr size_t TAIL_SIZE = 2;
constexpr size_t MIN_PACKET_SIZE = HEADER_SIZE + TAIL_SIZE;  // Data 없을 때

// Job Codes
constexpr char JOB_CODE_DEVICE_CHECK = 'A';
constexpr char JOB_CODE_DEVICE_CHECK_RESPONSE = 'a';
constexpr char JOB_CODE_PAYMENT_WAIT = 'E';
constexpr char JOB_CODE_PAYMENT_WAIT_RESPONSE = 'e';
constexpr char JOB_CODE_CARD_UID_READ = 'F';
constexpr char JOB_CODE_CARD_UID_READ_RESPONSE = 'f';
constexpr char JOB_CODE_EVENT = '@';  // 이벤트 전문
constexpr char JOB_CODE_RESET = 'R';
constexpr char JOB_CODE_RESET_RESPONSE = 'r';
constexpr char JOB_CODE_PAYMENT_APPROVAL = 'B';
constexpr char JOB_CODE_PAYMENT_APPROVAL_RESPONSE = 'b';
constexpr char JOB_CODE_LAST_APPROVAL_RESPONSE = 'L';
constexpr char JOB_CODE_LAST_APPROVAL_RESPONSE_RESPONSE = 'l';
constexpr char JOB_CODE_SCREEN_SOUND_SETTING = 'S';
constexpr char JOB_CODE_SCREEN_SOUND_SETTING_RESPONSE = 's';
constexpr char JOB_CODE_IC_CARD_CHECK = 'M';
constexpr char JOB_CODE_IC_CARD_CHECK_RESPONSE = 'm';

// 장치체크 응답 구조
struct DeviceCheckResponse {
    char cardModuleStatus;      // N/O/X
    char rfModuleStatus;         // O/X
    char vanServerStatus;        // N/O/X/F
    char integrationServerStatus; // N/O/X/F
};

// 결제대기 응답 구조 (문서에 명시되지 않음, 기본 구조)
struct PaymentWaitResponse {
    std::vector<uint8_t> data;  // 응답 데이터 (형식 미확정)
};

// 카드 UID 읽기 응답 구조 (문서에 명시되지 않음, 기본 구조)
struct CardUidReadResponse {
    std::vector<uint8_t> uid;  // 카드 UID (일반적으로 4-8 bytes)
};

// 이벤트 타입
enum class EventType {
    MS_CARD_DETECTED,      // @M - MS 카드 인식
    RF_CARD_DETECTED,      // @R - RF 카드 인식
    IC_CARD_DETECTED,      // @I - IC 카드 인식
    IC_CARD_REMOVED,       // @O - IC 카드 제거
    IC_CARD_FALLBACK,      // @F - IC 카드 Fallback
    UNKNOWN                // 알 수 없는 이벤트
};

// 이벤트 응답 구조
struct EventResponse {
    EventType type;              // 이벤트 타입
    std::vector<uint8_t> data;  // 이벤트 데이터 (문서에 명시되지 않음)
};

// 거래승인 요청 구조
struct PaymentApprovalRequest {
    uint8_t transactionType;    // 1: 승인, 2: 마지막 거래 취소
    uint32_t amount;            // 승인금액 (원)
    uint32_t tax;               // 세금 (원)
    uint32_t service;           // 봉사료 (원)
    uint8_t installments;       // 할부개월 (00: 일시불)
    uint8_t signatureRequired;  // 1: 비서명, 2: 서명
};

// 거래승인 응답 구조 (문서에 명시되지 않음, 기본 구조)
struct PaymentApprovalResponse {
    std::vector<uint8_t> data;  // 응답 데이터 (157 bytes)
};

// 마지막 승인 응답 구조 (B 응답과 동일)
struct LastApprovalResponse {
    std::vector<uint8_t> data;  // 응답 데이터 (157 bytes, same as PaymentApprovalResponse)
};

// 화면/음성 설정 요청 구조
struct ScreenSoundSettingRequest {
    uint8_t screenBrightness;   // 0-9
    uint8_t soundVolume;        // 0-9
    uint8_t touchSoundVolume;   // 0-9
};

// 화면/음성 설정 응답 구조
struct ScreenSoundSettingResponse {
    uint8_t screenBrightness;   // 설정된 화면밝기
    uint8_t soundVolume;        // 설정된 음성볼륨
    uint8_t touchSoundVolume;   // 설정된 터치음볼륨
};

// IC 카드 체크 응답 구조
struct IcCardCheckResponse {
    char cardStatus;  // 'O': IC 카드 삽입, 'X': IC 카드 없음
};

class SmartroProtocol {
public:
    // 장치체크 요청 패킷 생성
    static std::vector<uint8_t> createDeviceCheckRequest(const std::string& terminalId);
    
    // 결제대기 요청 패킷 생성
    static std::vector<uint8_t> createPaymentWaitRequest(const std::string& terminalId);
    
    // 카드 UID 읽기 요청 패킷 생성
    static std::vector<uint8_t> createCardUidReadRequest(const std::string& terminalId);
    
    // 단말기 리셋 요청 패킷 생성
    static std::vector<uint8_t> createResetRequest(const std::string& terminalId);
    
    // 거래승인 요청 패킷 생성
    static std::vector<uint8_t> createPaymentApprovalRequest(const std::string& terminalId,
                                                              const PaymentApprovalRequest& request);
    
    // 마지막 승인 응답 요청 패킷 생성
    static std::vector<uint8_t> createLastApprovalResponseRequest(const std::string& terminalId);
    
    // 화면/음성 설정 요청 패킷 생성
    static std::vector<uint8_t> createScreenSoundSettingRequest(const std::string& terminalId,
                                                                const ScreenSoundSettingRequest& request);
    
    // IC 카드 체크 요청 패킷 생성
    static std::vector<uint8_t> createIcCardCheckRequest(const std::string& terminalId);
    
    // 패킷 파싱
    static bool parsePacket(const uint8_t* data, size_t length, 
                           std::vector<uint8_t>& header, 
                           std::vector<uint8_t>& payload);
    
    // BCC 계산 (STX부터 ETX까지 XOR)
    static uint8_t calculateBCC(const uint8_t* data, size_t length);
    
    // BCC 검증
    static bool verifyBCC(const uint8_t* packet, size_t packetLength);
    
    // 장치체크 응답 파싱
    static bool parseDeviceCheckResponse(const uint8_t* data, size_t length, 
                                        DeviceCheckResponse& response);
    
    // 결제대기 응답 파싱
    static bool parsePaymentWaitResponse(const uint8_t* data, size_t length, 
                                         PaymentWaitResponse& response);
    
    // 카드 UID 읽기 응답 파싱
    static bool parseCardUidReadResponse(const uint8_t* data, size_t length, 
                                         CardUidReadResponse& response);
    
    // 이벤트 응답 파싱
    static bool parseEventResponse(const uint8_t* data, size_t length, 
                                  EventResponse& response);
    
    // 거래승인 응답 파싱
    static bool parsePaymentApprovalResponse(const uint8_t* data, size_t length, 
                                            PaymentApprovalResponse& response);
    
    // 마지막 승인 응답 파싱 (B 응답과 동일)
    static bool parseLastApprovalResponse(const uint8_t* data, size_t length, 
                                         LastApprovalResponse& response);
    
    // 화면/음성 설정 응답 파싱
    static bool parseScreenSoundSettingResponse(const uint8_t* data, size_t length, 
                                               ScreenSoundSettingResponse& response);
    
    // IC 카드 체크 응답 파싱
    static bool parseIcCardCheckResponse(const uint8_t* data, size_t length, 
                                        IcCardCheckResponse& response);
    
    // DateTime 생성 (YYYYMMDDhhmmss)
    static std::string getCurrentDateTime();
    
    // Terminal ID 포맷팅 (16 bytes, 좌측 정렬, 나머지 0x00)
    static std::vector<uint8_t> formatTerminalId(const std::string& terminalId);
    
    // USHORT를 Little Endian으로 변환
    static void writeUshortLE(uint16_t value, uint8_t* buffer);
    
    // Little Endian USHORT 읽기
    static uint16_t readUshortLE(const uint8_t* buffer);
    
    // 패킷에서 Data Length 추출
    static uint16_t extractDataLength(const uint8_t* header);
    
    // Job Code 추출
    static char extractJobCode(const uint8_t* header);
};

} // namespace smartro
