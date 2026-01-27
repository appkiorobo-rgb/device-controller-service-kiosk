// include/vendor_adapters/smartro/smartro_comm.h
#pragma once

#include "vendor_adapters/smartro/serial_port.h"
#include "vendor_adapters/smartro/smartro_protocol.h"
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

// 응답 타입 (Job Code로 구분)
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

// 통합 응답 구조체
struct ResponseData {
    ResponseType type;
    char jobCode;
    std::vector<uint8_t> rawData;  // 원본 데이터
    
    // 각 타입별 데이터 (타입에 맞는 것만 사용)
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
    
    // 응답 수신 스레드 시작/중지
    void startResponseReceiver();
    void stopResponseReceiver();
    
    // 응답 폴링 (비동기 응답 가져오기)
    bool pollResponse(ResponseData& response, uint32_t timeoutMs = 0);
    
    // 기존 동기 방식 함수들 (하위 호환성 유지)
    // 장치체크 요청 전송 및 응답 수신
    bool sendDeviceCheckRequest(const std::string& terminalId, 
                                DeviceCheckResponse& response,
                                uint32_t timeoutMs = 3000);
    
    // 결제대기 요청 전송 및 응답 수신
    bool sendPaymentWaitRequest(const std::string& terminalId, 
                                PaymentWaitResponse& response,
                                uint32_t timeoutMs = 3000);
    
    // 카드 UID 읽기 요청 전송 및 응답 수신
    bool sendCardUidReadRequest(const std::string& terminalId, 
                                CardUidReadResponse& response,
                                uint32_t timeoutMs = 3000);
    
    // 이벤트 대기 (블로킹, 타임아웃 가능)
    // 이벤트는 기기에서 자동으로 전송되므로 ACK/NACK 미전송
    bool waitForEvent(EventResponse& event, uint32_t timeoutMs = 0);  // timeoutMs=0이면 무한 대기
    
    // 단말기 리셋 요청 전송 및 응답 수신
    bool sendResetRequest(const std::string& terminalId, uint32_t timeoutMs = 3000);
    
    // 거래승인 요청 전송 (비동기 - 요청만 보내고 바로 반환)
    bool sendPaymentApprovalRequestAsync(const std::string& terminalId, 
                                        const PaymentApprovalRequest& request);
    
    // 거래승인 요청 전송 및 응답 수신 (동기 - 하위 호환성)
    bool sendPaymentApprovalRequest(const std::string& terminalId, 
                                    const PaymentApprovalRequest& request,
                                    PaymentApprovalResponse& response,
                                    uint32_t timeoutMs = 30000);  // 결제는 시간이 더 걸릴 수 있음
    
    // 마지막 승인 응답 요청 전송 및 응답 수신
    bool sendLastApprovalResponseRequest(const std::string& terminalId, 
                                        LastApprovalResponse& response,
                                        uint32_t timeoutMs = 30000);
    
    // 화면/음성 설정 요청 전송 및 응답 수신
    bool sendScreenSoundSettingRequest(const std::string& terminalId, 
                                       const ScreenSoundSettingRequest& request,
                                       ScreenSoundSettingResponse& response,
                                       uint32_t timeoutMs = 3000);
    
    // IC 카드 체크 요청 전송 및 응답 수신
    bool sendIcCardCheckRequest(const std::string& terminalId, 
                               IcCardCheckResponse& response,
                               uint32_t timeoutMs = 3000);
    
    // 상태 조회
    CommState getState() const { return state_; }
    std::string getLastError() const { return lastError_; }
    
private:
    SerialPort& serialPort_;
    CommState state_;
    std::string lastError_;
    mutable std::mutex commMutex_;  // 멀티스레드 안전을 위한 락
    
    // 비동기 응답 수신 관련
    std::atomic<bool> receiverRunning_;
    std::thread receiverThread_;
    std::queue<ResponseData> responseQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    static constexpr uint32_t ACK_TIMEOUT_MS = 5000;  // ACK 대기 타임아웃
    static constexpr uint32_t RESPONSE_TIMEOUT_MS = 10000;  // 응답 수신 타임아웃
    
    // 백그라운드 응답 수신 스레드
    void responseReceiverThread();
    
    // 응답 파싱 및 큐에 추가
    void processResponse(const std::vector<uint8_t>& packet);
    
    // ACK/NACK 처리
    bool waitForAck(uint32_t timeoutMs, std::vector<uint8_t>& responsePacket);
    bool sendAck();
    bool sendNack();
    
    // 응답 수신
    bool receiveResponse(std::vector<uint8_t>& responsePacket, uint32_t timeoutMs);
    
    // 단일 바이트 읽기 (ACK/NACK용)
    bool readByte(uint8_t& byte, uint32_t timeoutMs);
    
    // Serial 버퍼 비우기
    void flushSerialBuffer();
    
    // 에러 설정
    void setError(const std::string& error);
};

} // namespace smartro
