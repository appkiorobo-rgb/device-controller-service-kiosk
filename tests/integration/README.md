# 결제 단말기 테스트 가이드

Flutter 없이 결제 단말기 기능을 테스트하는 방법입니다.

## 테스트 클라이언트

두 가지 테스트 클라이언트가 제공됩니다:

1. **Python 클라이언트** (`test_payment_client.py`) - 권장
2. **C++ 클라이언트** (`test_payment_client.cpp`)

---

## Python 클라이언트 사용법

### 1. 필수 패키지 설치

```bash
pip install pywin32
```

### 2. 사용 예시

#### 상태 확인
```bash
python test_payment_client.py status
```

#### 결제 시작 (10,000원)
```bash
python test_payment_client.py start 10000
```

#### 결제 취소
```bash
python test_payment_client.py cancel
```

#### 단말기 리셋
```bash
python test_payment_client.py reset
```

#### 장치 체크
```bash
python test_payment_client.py check
```

#### 이벤트 수신 (30초 대기)
```bash
python test_payment_client.py listen 30
```

### 3. 전체 테스트 시나리오

```bash
# 1. 상태 확인
python test_payment_client.py status

# 2. 결제 시작
python test_payment_client.py start 10000

# 3. 별도 터미널에서 이벤트 수신
python test_payment_client.py listen 60

# 4. (필요시) 결제 취소
python test_payment_client.py cancel

# 5. (오류 발생시) 리셋
python test_payment_client.py reset
```

---

## C++ 클라이언트 사용법

### 1. 빌드

CMakeLists.txt에 테스트 클라이언트를 추가하거나 직접 컴파일:

```bash
# nlohmann/json 필요
g++ -std=c++17 test_payment_client.cpp -o test_payment_client.exe -lws2_32
```

### 2. 사용 예시

```bash
# 상태 확인
test_payment_client.exe status

# 결제 시작
test_payment_client.exe start 10000

# 결제 취소
test_payment_client.exe cancel

# 단말기 리셋
test_payment_client.exe reset

# 장치 체크
test_payment_client.exe check

# 이벤트 수신
test_payment_client.exe listen 30
```

---

## 테스트 시나리오

### 시나리오 1: 정상 결제 플로우

**터미널 1:**
```bash
# 상태 확인
python test_payment_client.py status
# 출력: State: READY

# 결제 시작
python test_payment_client.py start 10000
# 출력: State: PROCESSING
```

**터미널 2:**
```bash
# 이벤트 수신 대기
python test_payment_client.py listen 60
```

**예상 이벤트:**
1. `payment_state_changed` → state: PROCESSING
2. (카드 인식 시) `payment_state_changed` → state: PROCESSING
3. `payment_complete` → transactionId, amount 포함
   또는
   `payment_failed` → errorCode, errorMessage 포함

### 시나리오 2: 결제 취소

```bash
# 결제 시작
python test_payment_client.py start 10000

# (다른 터미널에서) 이벤트 수신 시작
python test_payment_client.py listen

# 결제 취소
python test_payment_client.py cancel

# 예상 이벤트: payment_cancelled
```

### 시나리오 3: 오류 복구

```bash
# 상태 확인 (ERROR 상태일 경우)
python test_payment_client.py status
# 출력: State: ERROR

# 리셋
python test_payment_client.py reset

# 상태 재확인
python test_payment_client.py status
# 출력: State: READY
```

---

## 응답 예시

### 상태 확인 응답
```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": 2,
    "deviceName": "SMARTRO Payment Terminal"
  }
}
```

### 결제 시작 응답
```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": 3
  }
}
```

### 이벤트 예시
```json
{
  "protocolVersion": "1.0",
  "kind": "event",
  "eventId": "uuid-here",
  "eventType": "payment_complete",
  "timestampMs": 1234567890,
  "deviceType": "payment",
  "data": {
    "transactionId": "202401261234567890",
    "amount": 10000,
    "state": 2
  }
}
```

---

## 문제 해결

### 연결 실패
- Device Controller Service가 실행 중인지 확인
- 서비스가 시작되었는지 확인: `sc query DeviceControllerService`
- Named Pipe 이름이 올바른지 확인

### 응답 없음
- 서비스 로그 확인: `C:\ProgramData\DeviceControllerService\logs\service.log`
- 결제 단말기가 연결되어 있는지 확인
- COM 포트가 올바른지 확인

### 상태가 READY가 아님
- `payment_reset` 명령으로 리셋 시도
- 서비스 재시작 고려
- 결제 단말기 하드웨어 확인

---

## 자동화 테스트 스크립트

간단한 자동화 테스트:

```bash
#!/bin/bash
# test_automated.sh

echo "=== Payment Terminal Automated Test ==="

# 1. 상태 확인
echo "1. Checking status..."
python test_payment_client.py status

# 2. 결제 시작
echo "2. Starting payment..."
python test_payment_client.py start 10000

# 3. 잠시 대기
echo "3. Waiting 5 seconds..."
sleep 5

# 4. 상태 재확인
echo "4. Checking status again..."
python test_payment_client.py status

echo "=== Test Complete ==="
```

---

## 참고

- 모든 명령어는 `commandId`를 UUID로 생성합니다
- 동일한 `commandId`로 재전송하면 이전 응답이 반환됩니다 (Idempotency)
- 이벤트는 비동기로 수신되므로 별도 스레드/프로세스에서 수신해야 합니다
- 실제 결제 단말기가 연결되어 있어야 정상 동작합니다
