# LV77 / ICT-104U 지폐식별기 프로토콜

- **규격**: ICT Protocol For RS232 (V0.2/V0.3)
- **통신**: 9600 baud, **8 data bits, Even parity, 1 stop bit (8E1)**
- **방식**: 단일 바이트 명령/응답 (풀듀플렉스)

## Controller → Bill Acceptor (호스트 → 장비)

| 코드 | 설명                                                 |
| ---- | ---------------------------------------------------- |
| 0x02 | Sync ACK (전원 인가 후 2초 이내 전송 시 장비 Enable) / **3.2 Escrow 수락 시에도 02H 전송** |
| 0x0C | 상태 폴링 (5초 이내 주기적으로 전송 필요)            |
| 0x0F | 에스크로 지폐 반환(Reject)                           |
| 0x10 | 스택 명령 (일부 규격; ICT104U 3.2 Escrow 수락은 0x02 사용) |
| 0x11 | 반환(Reject)                                         |
| 0x18 | 에스크로 홀드                                        |
| 0x30 | 리셋                                                 |
| 0x3E | Enable                                               |
| 0x5E | Disable                                              |
| 0x5A | Escrow Hold (V0.3)                                   |

## Bill Acceptor → Controller (장비 → 호스트)

| 코드      | 설명                                                      |
| --------- | --------------------------------------------------------- |
| 0x80      | 전원 인가                                                 |
| 0x8F      | 0x02 수신 후 응답                                         |
| 0x81      | 지폐 검증됨(에스크로), 다음 바이트가 지폐 종류(0x40~0x44) |
| 0x40~0x44 | 지폐 종류(1~5번)                                          |
| 0x10      | 스태킹 중                                                 |
| 0x11      | 반환 중                                                   |

## 폴 응답(0x0C에 대한 응답)

- 0x20 Restart BA, 0x21 Motor Failure, 0x22 Checksum Error, 0x23 Bill Jam, 0x24 Bill Remove, 0x25 Stacker Open, 0x27 Sensor Problem, 0x28 Bill Fish, 0x29 Stacker Problem, 0x2A Bill Reject, 0x3E Enable, 0x5E Inhibit 등

## 구현 위치

- `include/vendor_adapters/lv77/lv77_protocol.h` – 상수·유틸
- `include/vendor_adapters/lv77/lv77_comm.h` / `src/.../lv77_comm.cpp` – 시리얼 통신·폴 루프
- `include/vendor_adapters/lv77/lv77_bill_adapter.h` / `src/.../lv77_bill_adapter.cpp` – IPaymentTerminal 어댑터

## 3.2 Escrow 구현

- **흐름**: 폴 수신 시 장비가 `0x81`(지폐 검증) + 지폐코드(`0x40~0x44`) 전송 → 호스트가 **02H**(0x02, 수락) 또는 **0x0F**(반환) 전송. 02H를 보내야 지폐가 수락되고, 안 보내면 지폐가 그대로 나온다.
- **구현**: `lv77_comm.cpp` 폴 루프에서 `RSP_BILL_VALIDATED` 수신 시 다음 바이트 읽어 지폐 종류 확인 후 수락 시 `CMD_SYNC_ACK`(0x02), 반환 시 `CMD_REJECT_BILL`(0x0F) 전송.
- **테스트 모드**: `startPayment(0)` 시 목표 0원 → 전 수락.
- **잔돈 없음**: `startPayment(target)` 시 `currentTotal + 지폐액 <= target`이면 수락, 초과면 해당 지폐 반환(0x0F) 후 `payment_failed` 이벤트로 Flutter 전달 (`errorCode`: `CASH_BILL_RETURNED`, `amount`: 반환된 지폐 액면).

## 테스트 클라이언트 (exe)

```bash
# 빌드 후
test_lv77_client.exe [COM포트]
# 예: test_lv77_client.exe COM4
```

- COM 포트를 9600 8E1로 열고, Sync(0x02) → Enable(0x3E) → 폴(0x0C) 루프.
- 지폐 인식 시 0x81 → 지폐코드 수신 → 0x10(수락) 전송.
