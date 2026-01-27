# SMARTRO Payment Terminal Implementation Summary

## 개요

이 문서는 SMARTRO 결제 단말기(TL3500/TL3010)와의 통신을 위한 C++ 구현 내용을 정리한 것입니다.

**구현 일자**: 2026-01-26  
**프로토콜 버전**: v3.5  
**통신 방식**: Serial (COM Port)

---

## 1. 아키텍처 개요

### 1.1 레이어 구조

```
┌─────────────────────────────────────┐
│   Application Layer (Test/Service)   │
├─────────────────────────────────────┤
│   SmartroComm (Communication)       │  ← 요청 전송, 응답 수신 관리
├─────────────────────────────────────┤
│   SmartroProtocol (Protocol)        │  ← 패킷 생성/파싱, BCC 계산
├─────────────────────────────────────┤
│   SerialPort (Hardware)             │  ← Windows Serial API 래퍼
└─────────────────────────────────────┘
```

### 1.2 주요 컴포넌트

- **SerialPort**: Windows COM 포트 통신 래퍼
- **SmartroProtocol**: 프로토콜 패킷 생성/파싱, BCC 계산
- **SmartroComm**: 통신 흐름 관리 (ACK/NACK, 타임아웃, 재시도)
- **Logger**: 파일 기반 로깅 시스템

---

## 2. 프로토콜 구현 (SmartroProtocol)

### 2.1 패킷 구조

```
[STX(1)] + [Header(34)] + [Data(N)] + [ETX(1)] + [BCC(1)]
```

**Header 구조 (34 bytes)**:
- Terminal ID: 16 bytes (좌측 정렬, 나머지 0x00)
- DateTime: 14 bytes (YYYYMMDDhhmmss)
- Job Code: 1 byte
- Response Code: 1 byte (요청 시 0x00)
- Data Length: 2 bytes (Little Endian, USHORT)

### 2.2 BCC 계산

```cpp
uint8_t calculateBCC(const uint8_t* data, size_t length) {
    uint8_t bcc = 0;
    for (size_t i = 0; i < length; ++i) {
        bcc ^= data[i];
    }
    return bcc;
}
```

**중요**: BCC는 **STX부터 ETX까지** 포함하여 계산합니다.

### 2.3 패킷 생성 함수

모든 요청 패킷 생성 함수는 다음 패턴을 따릅니다:

```cpp
std::vector<uint8_t> createXXXRequest(const std::string& terminalId, ...) {
    std::vector<uint8_t> packet;
    
    // STX
    packet.push_back(STX);
    
    // Terminal ID (16 bytes, 좌측 정렬)
    auto formattedTerminalId = formatTerminalId(terminalId);
    packet.insert(packet.end(), formattedTerminalId.begin(), formattedTerminalId.end());
    
    // DateTime (14 bytes, YYYYMMDDhhmmss)
    std::string dateTime = getCurrentDateTime();
    for (size_t i = 0; i < 14; ++i) {
        if (i < dateTime.length()) {
            packet.push_back(static_cast<uint8_t>(dateTime[i]));
        } else {
            packet.push_back(0x00);
        }
    }
    
    // Job Code
    packet.push_back(static_cast<uint8_t>(JOB_CODE_XXX));
    
    // Response Code (요청 시 0x00)
    packet.push_back(0x00);
    
    // Data Length (Little Endian, USHORT)
    uint16_t dataLength = /* 계산된 길이 */;
    packet.push_back(static_cast<uint8_t>(dataLength & 0xFF));
    packet.push_back(static_cast<uint8_t>((dataLength >> 8) & 0xFF));
    
    // Data (Job Code에 따라 다름)
    // ... 데이터 추가 ...
    
    // ETX
    packet.push_back(ETX);
    
    // BCC 계산 및 추가
    uint8_t bcc = calculateBCC(packet.data(), packet.size());
    packet.push_back(bcc);
    
    return packet;
}
```

### 2.4 패킷 파싱 함수

```cpp
bool parsePacket(const uint8_t* data, size_t length,
                 std::vector<uint8_t>& header,
                 std::vector<uint8_t>& payload) {
    // 1. STX 확인
    if (data[0] != STX) return false;
    
    // 2. 최소 크기 확인 (Header + ETX + BCC)
    if (length < MIN_PACKET_SIZE) return false;
    
    // 3. Header 추출 (STX 제외, 34 bytes)
    header.assign(data + 1, data + 1 + HEADER_SIZE);
    
    // 4. Data Length 추출 (Little Endian)
    uint16_t dataLength = static_cast<uint16_t>(header[HEADER_SIZE - 2]) |
                         (static_cast<uint16_t>(header[HEADER_SIZE - 1]) << 8);
    
    // 5. 전체 패킷 크기 확인
    size_t expectedSize = 1 + HEADER_SIZE + dataLength + 1 + 1;  // STX + Header + Data + ETX + BCC
    if (length < expectedSize) return false;
    
    // 6. ETX 확인
    if (data[expectedSize - 2] != ETX) return false;
    
    // 7. BCC 검증
    uint8_t calculatedBCC = calculateBCC(data, expectedSize - 1);
    if (calculatedBCC != data[expectedSize - 1]) return false;
    
    // 8. Payload 추출
    if (dataLength > 0) {
        payload.assign(data + 1 + HEADER_SIZE, data + 1 + HEADER_SIZE + dataLength);
    }
    
    return true;
}
```

---

## 3. 구현된 명령어

### 3.1 장치 체크 (A → a)

**요청**:
- Job Code: 'A'
- Data Length: 0

**응답**:
- Job Code: 'a'
- Data Length: 4 bytes
- 데이터 구조:
  ```cpp
  struct DeviceCheckResponse {
      char cardModuleStatus;        // 'N': 미설치, 'O': 정상, 'X': 오류
      char rfModuleStatus;          // 'O': 정상, 'X': 오류
      char vanServerStatus;         // 'N', 'O', 'X', 'F'
      char integrationServerStatus;  // 'N', 'O', 'X', 'F'
  };
  ```

### 3.2 결제 대기 (E → e)

**요청**:
- Job Code: 'E'
- Data Length: 0

**응답**:
- Job Code: 'e'
- Data Length: 0 (프로토콜 명세에 따라)

### 3.3 카드 UID 읽기 (F → f)

**요청**:
- Job Code: 'F'
- Data Length: 0

**응답**:
- Job Code: 'f'
- Data Length: 가변 (카드가 없으면 0)
- 데이터: UID 바이트 배열

### 3.4 단말기 리셋 (R → r)

**요청**:
- Job Code: 'R'
- Data Length: 0

**응답**:
- Job Code: 'r'
- Data Length: 0

### 3.5 거래 승인 (B → b)

**요청**:
- Job Code: 'B'
- Data Length: 30 bytes
- 데이터 구조:
  ```cpp
  struct PaymentApprovalRequest {
      uint8_t transactionType;      // 1: 승인, 2: 취소
      uint32_t amount;               // 승인 금액 (원)
      uint32_t tax;                  // 세금 (원)
      uint32_t service;              // 봉사료 (원)
      uint8_t installments;          // 할부 개월 (00: 일시불)
      uint8_t signatureRequired;     // 1: 비서명, 2: 서명
  };
  ```
- 데이터 포맷:
  - 거래구분코드: 1 byte (CHAR, "1" 또는 "2")
  - 승인금액: 10 bytes (CHAR, 우측 정렬, 좌측 "0" 패딩)
  - 세금: 8 bytes (CHAR, 우측 정렬, 좌측 "0" 패딩)
  - 봉사료: 8 bytes (CHAR, 우측 정렬, 좌측 "0" 패딩)
  - 할부개월: 2 bytes (CHAR, 우측 정렬, 좌측 "0" 패딩)
  - 서명여부: 1 byte (CHAR, "1" 또는 "2")

**응답**:
- Job Code: 'b'
- Data Length: 157 bytes
- 데이터: 원본 바이트 배열 (파싱 로직은 추후 구현)

### 3.6 마지막 승인 응답 요청 (L → l)

**요청**:
- Job Code: 'L'
- Data Length: 0

**응답**:
- Job Code: 'l'
- Data Length: 157 bytes (B 응답과 동일)

### 3.7 화면/음성 설정 (S → s)

**요청**:
- Job Code: 'S'
- Data Length: 3 bytes
- 데이터 구조:
  ```cpp
  struct ScreenSoundSettingRequest {
      uint8_t screenBrightness;   // 0-9 (CHAR)
      uint8_t soundVolume;        // 0-9 (CHAR)
      uint8_t touchSoundVolume;   // 0-9 (CHAR)
  };
  ```

**응답**:
- Job Code: 's'
- Data Length: 3 bytes
- 데이터 구조:
  ```cpp
  struct ScreenSoundSettingResponse {
      uint8_t screenBrightness;   // 설정된 값
      uint8_t soundVolume;        // 설정된 값
      uint8_t touchSoundVolume;   // 설정된 값
  };
  ```

### 3.8 IC 카드 체크 (M → m)

**요청**:
- Job Code: 'M'
- Data Length: 0

**응답**:
- Job Code: 'm'
- Data Length: 1 byte
- 데이터 구조:
  ```cpp
  struct IcCardCheckResponse {
      char cardStatus;  // 'O': IC 카드 삽입, 'X': IC 카드 없음
  };
  ```

### 3.9 이벤트 수신 (@)

**특징**:
- 기기에서 자동으로 전송 (요청 없음)
- ACK/NACK 전송 불필요
- Job Code: '@'
- 이벤트 타입:
  - '@M': MS 카드 감지
  - '@R': RF 카드 감지
  - '@I': IC 카드 삽입
  - '@O': IC 카드 제거
  - '@F': IC 카드 Fallback

---

## 4. 통신 흐름 (SmartroComm)

### 4.1 동기 통신 흐름

```
1. 요청 패킷 생성
2. Serial 버퍼 비우기 (이전 데이터 제거)
3. 요청 패킷 전송
4. ACK 대기 (타임아웃: 5초)
   - ACK 수신 시: 응답 패킷 시작(STX) 확인
   - NACK 수신 시: 실패
5. 응답 패킷 수신 (타임아웃: 10초)
6. 응답 파싱 및 검증
7. ACK 전송
8. 완료
```

### 4.2 ACK/NACK 처리

- **ACK (0x06)**: 요청 수신 확인, 응답 전송 예정
- **NACK (0x15)**: 요청 거부 또는 오류
- **타임아웃**: 5초 내 ACK/NACK 미수신 시 실패

### 4.3 응답 수신 최적화

**문제**: ACK 수신 직후 응답 패킷(STX)이 바로 올 수 있음

**해결**:
```cpp
bool waitForAck(uint32_t timeoutMs, std::vector<uint8_t>& responsePacket) {
    // ACK 수신
    if (readByte(byte, timeoutMs) && byte == ACK) {
        // ACK 수신 후 짧은 타임아웃으로 STX 확인
        uint8_t nextByte = 0;
        if (readByte(nextByte, 1000)) {  // 1초 타임아웃
            if (nextByte == STX) {
                responsePacket.push_back(STX);  // 미리 저장
            }
        }
        return true;
    }
    return false;
}

bool receiveResponse(std::vector<uint8_t>& responsePacket, uint32_t timeoutMs) {
    // 이미 STX를 받았는지 확인
    bool foundStx = !responsePacket.empty() && responsePacket[0] == STX;
    
    if (!foundStx) {
        // STX 찾기
        // ...
    }
    
    // 나머지 패킷 읽기
    // ...
}
```

### 4.4 버퍼 플러시 타이밍

**중요**: 버퍼 플러시는 **요청 전송 전**에 수행합니다.

```cpp
// 올바른 순서
flushSerialBuffer();  // 이전 데이터 제거
serialPort_.write(packet.data(), packet.size());  // 요청 전송
waitForAck(...);  // ACK 대기
```

이렇게 하면:
- 이전 응답 데이터만 제거
- 새로 들어오는 응답은 보존

---

## 5. 비동기 패턴 구현

### 5.1 설계 목적

**문제**: 결제 요청(B) 전송 후 응답 대기 중에 취소 요청(E)을 보낼 수 없음

**해결**: 요청 전송과 응답 수신을 분리

### 5.2 구조

```
┌─────────────────────────────────────┐
│   Main Thread                        │
│   - sendPaymentApprovalRequestAsync()│  ← 요청만 보내고 바로 반환
│   - sendPaymentWaitRequest()         │  ← 취소 요청 즉시 전송 가능
└─────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────┐
│   Response Receiver Thread          │
│   - 계속 STX 감지                    │
│   - 패킷 수신                        │
│   - 파싱 및 큐에 추가                │
└─────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────┐
│   Response Queue                    │
│   - Job Code로 구분                  │
│   - pollResponse()로 가져오기        │
└─────────────────────────────────────┘
```

### 5.3 주요 함수

#### 5.3.1 응답 수신 스레드 시작/중지

```cpp
void startResponseReceiver();
void stopResponseReceiver();
```

#### 5.3.2 비동기 요청 전송

```cpp
bool sendPaymentApprovalRequestAsync(const std::string& terminalId, 
                                    const PaymentApprovalRequest& request);
```

**동작**:
1. 요청 패킷 생성
2. 전송 및 ACK 대기
3. ACK 전송
4. **즉시 반환** (응답은 백그라운드에서 수신)

#### 5.3.3 응답 폴링

```cpp
bool pollResponse(ResponseData& response, uint32_t timeoutMs = 0);
```

**동작**:
- 큐에서 응답 가져오기
- `timeoutMs = 0`: 무한 대기
- `timeoutMs > 0`: 타임아웃 설정

#### 5.3.4 응답 데이터 구조

```cpp
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

struct ResponseData {
    ResponseType type;
    char jobCode;
    std::vector<uint8_t> rawData;  // 원본 데이터
    
    // 타입별 데이터 (union처럼 사용)
    DeviceCheckResponse deviceCheck;
    PaymentWaitResponse paymentWait;
    CardUidReadResponse cardUid;
    PaymentApprovalResponse paymentApproval;
    LastApprovalResponse lastApproval;
    ScreenSoundSettingResponse screenSound;
    IcCardCheckResponse icCard;
    EventResponse event;
};
```

### 5.4 사용 예시

```cpp
// 1. 응답 수신 스레드 시작
comm.startResponseReceiver();

// 2. 비동기로 결제 요청 전송
PaymentApprovalRequest request;
request.amount = 10000;
// ... 설정 ...
comm.sendPaymentApprovalRequestAsync(terminalId, request);

// 3. 바로 취소 요청 전송 가능!
PaymentWaitResponse cancelResponse;
comm.sendPaymentWaitRequest(terminalId, cancelResponse);

// 4. 응답 폴링
ResponseData response;
while (comm.pollResponse(response, 1000)) {  // 1초 타임아웃
    switch (response.type) {
        case ResponseType::PAYMENT_APPROVAL:
            // 결제 응답 처리
            break;
        case ResponseType::PAYMENT_WAIT:
            // 취소 응답 처리
            break;
        // ...
    }
}

// 5. 종료 시 스레드 중지
comm.stopResponseReceiver();
```

---

## 6. 멀티스레드 안전성

### 6.1 락 구조

- **commMutex_**: SerialPort 접근 보호
- **queueMutex_**: 응답 큐 접근 보호
- **queueCondition_**: 응답 대기 조건 변수

### 6.2 동기화 전략

**요청 전송 함수들**:
- 전체 함수 실행 동안 `commMutex_` 보유
- SerialPort는 단일 스레드에서만 접근

**응답 수신 스레드**:
- STX 감지 시에만 `commMutex_` 보유 (짧은 시간)
- 패킷 읽기 시에도 `commMutex_` 보유
- 큐 추가 시 `queueMutex_` 보유

**응답 폴링**:
- `queueMutex_` 보유하여 큐 접근

---

## 7. 시리얼 포트 관리 (SerialPort)

### 7.1 COM 포트 자동 감지

```cpp
static std::vector<std::string> getAvailablePorts();
```

**구현**:
- Windows Registry에서 COM 포트 목록 읽기
- `HKEY_LOCAL_MACHINE\HARDWARE\DEVICEMAP\SERIALCOMM`

### 7.2 포트 저장/로드

```cpp
static bool saveWorkingPort(const std::string& portName);
static std::string loadWorkingPort();
```

**구현**:
- 파일: `smartro_port.cfg`
- 성공한 포트를 저장하여 다음 실행 시 재사용

### 7.3 통신 설정

- **Baud Rate**: 115200 (기본값)
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None

---

## 8. 로깅 시스템

### 8.1 로그 레벨

- **DEBUG**: 상세 디버깅 정보 (패킷 크기, 바이트 값 등)
- **INFO**: 일반 정보 (요청/응답 성공, 상태 변경)
- **WARN**: 경고 (타임아웃, 예상치 못한 바이트)
- **ERROR**: 오류 (통신 실패, 파싱 오류)

### 8.2 로그 포맷

```
[LEVEL] Message
```

**예시**:
```
[DEBUG] Created device check request packet: 37 bytes
[INFO] ACK received (0x06)
[WARN] ACK timeout or NACK received
[ERROR] SmartroComm error: Failed to receive response
```

### 8.3 Hex Dump

바이너리 데이터는 16진수로 로깅:

```cpp
std::string hexDump;
for (size_t i = 0; i < length && i < 64; ++i) {
    char hex[4];
    std::snprintf(hex, sizeof(hex), "%02X ", data[i]);
    hexDump += hex;
}
logging::Logger::getInstance().debug("Packet data: " + hexDump);
```

---

## 9. 타임아웃 설정

### 9.1 기본 타임아웃

- **ACK_TIMEOUT_MS**: 5000ms (5초)
- **RESPONSE_TIMEOUT_MS**: 10000ms (10초)
- **결제 요청 응답**: 30000ms (30초)

### 9.2 재시도 정책

**현재**: 재시도 없음 (1회만 시도)

**이유**:
- 결제 요청 중복 방지
- 타임아웃을 길게 설정하여 단일 시도로 충분

---

## 10. 에러 처리

### 10.1 에러 종류

1. **Serial Port 오류**: 포트 열기 실패, 읽기/쓰기 실패
2. **프로토콜 오류**: BCC 불일치, 잘못된 패킷 구조
3. **타임아웃**: ACK/응답 미수신
4. **파싱 오류**: 데이터 형식 불일치

### 10.2 에러 전달

```cpp
std::string getLastError() const;
```

모든 함수는 `bool` 반환:
- `true`: 성공
- `false`: 실패 (에러는 `getLastError()`로 확인)

---

## 11. 테스트 프로그램

### 11.1 통합 테스트 (test_integrated.cpp)

**기능**:
- 모든 명령을 메뉴로 선택 가능
- COM 포트 자동 감지
- 비동기 패턴 테스트 (C 메뉴)

**사용법**:
```bash
test_integrated.exe [COM_PORT] [TERMINAL_ID] [BAUD_RATE]
```

**메뉴**:
- A: Device Check
- B: Payment Approval (Sync)
- C: Payment Approval (Async) + Cancel Test
- E: Payment Wait
- F: Card UID Read
- L: Last Approval Response
- M: IC Card Check
- R: Reset Terminal
- S: Screen/Sound Setting
- @: Wait for Event
- Q: Quit

---

## 12. 향후 작업 (IPC 연동)

### 12.1 목표

Flutter 클라이언트와 IPC를 통한 통신

### 12.2 데이터 타입 정의 필요

**요청 타입**:
```cpp
enum class RequestType {
    DEVICE_CHECK,
    PAYMENT_APPROVAL,
    PAYMENT_WAIT,
    CARD_UID_READ,
    RESET,
    LAST_APPROVAL,
    SCREEN_SOUND_SETTING,
    IC_CARD_CHECK
};

struct IPCRequest {
    RequestType type;
    std::string requestId;  // 명령 ID (idempotency)
    std::string terminalId;
    // 타입별 데이터 (union 또는 variant)
};
```

**응답 타입**:
```cpp
struct IPCResponse {
    std::string requestId;
    bool success;
    std::string errorMessage;
    ResponseData data;  // 기존 ResponseData 재사용
};
```

**이벤트 타입**:
```cpp
struct IPCEvent {
    std::string eventId;
    ResponseData data;  // EventResponse 포함
};
```

### 12.3 IPC 채널

1. **Command/Response 채널**: 요청/응답 (Named Pipe 또는 TCP)
2. **Event 채널**: 비동기 이벤트 스트리밍

### 12.4 프로토콜 버전 관리

모든 IPC 메시지에 `protocolVersion` 필드 포함:
```cpp
struct IPCMessage {
    uint32_t protocolVersion;  // 현재: 1
    // ...
};
```

---

## 13. 파일 구조

```
include/vendor_adapters/smartro/
├── serial_port.h              # Serial 통신 래퍼
├── smartro_protocol.h         # 프로토콜 패킷 생성/파싱
└── smartro_comm.h            # 통신 흐름 관리

src/vendor_adapters/smartro/
├── serial_port.cpp
├── smartro_protocol.cpp
└── smartro_comm.cpp

include/logging/
└── logger.h                   # 로깅 시스템

src/logging/
└── logger.cpp

tests/
├── test_integrated.cpp        # 통합 테스트
├── test_device_check.cpp      # 장치 체크 테스트
├── test_payment_wait.cpp      # 결제 대기 테스트
└── test_card_uid_read.cpp     # 카드 UID 읽기 테스트
```

---

## 14. 주요 상수

### 14.1 프로토콜 상수

```cpp
constexpr uint8_t STX = 0x02;
constexpr uint8_t ETX = 0x03;
constexpr uint8_t ACK = 0x06;
constexpr uint8_t NACK = 0x15;

constexpr size_t HEADER_SIZE = 34;  // STX 제외
constexpr size_t MIN_PACKET_SIZE = 1 + HEADER_SIZE + 1 + 1;  // STX + Header + ETX + BCC
```

### 14.2 Job Code

```cpp
constexpr char JOB_CODE_DEVICE_CHECK = 'A';
constexpr char JOB_CODE_PAYMENT_WAIT = 'E';
constexpr char JOB_CODE_CARD_UID_READ = 'F';
constexpr char JOB_CODE_RESET = 'R';
constexpr char JOB_CODE_PAYMENT_APPROVAL = 'B';
constexpr char JOB_CODE_LAST_APPROVAL_RESPONSE = 'L';
constexpr char JOB_CODE_SCREEN_SOUND_SETTING = 'S';
constexpr char JOB_CODE_IC_CARD_CHECK = 'M';
constexpr char JOB_CODE_EVENT = '@';
```

---

## 15. 주의사항

### 15.1 BCC 계산

**중요**: BCC는 **STX부터 ETX까지** 포함하여 계산합니다.

```cpp
// 올바른 방법
uint8_t bcc = calculateBCC(packet.data(), packet.size() - 1);  // BCC 제외
```

### 15.2 버퍼 플러시 타이밍

버퍼 플러시는 **요청 전송 전**에 수행합니다.

### 15.3 응답 수신 최적화

ACK 수신 직후 STX가 올 수 있으므로, `waitForAck()`에서 미리 확인합니다.

### 15.4 멀티스레드 안전성

- 모든 SerialPort 접근은 `commMutex_` 보호
- 응답 큐 접근은 `queueMutex_` 보호

### 15.5 재시도 정책

결제 요청은 재시도하지 않습니다 (중복 결제 방지).

---

## 16. 참고 문서

- `docs/cursor/SMARTRO_PROTOCOL.md`: 프로토콜 명세서
- `docs/cursor/DEVICE_SERVICE_PLAYBOOK.md`: 서비스 아키텍처 가이드
- `docs/cursor/IPC_CONTRACT.md`: IPC 계약 (향후 작업)

---

**작성일**: 2026-01-26  
**작성자**: AI Assistant  
**버전**: 1.0
