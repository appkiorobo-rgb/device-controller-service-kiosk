# LV77 / ICT-104U 지폐식별기 프로토콜

- **규격**: ICT Protocol For RS232 (V0.2/V0.3)
- **통신**: 9600 baud, **8 data bits, Even parity, 1 stop bit (8E1)**
- **방식**: 단일 바이트 명령/응답 (풀듀플렉스)

## Controller → Bill Acceptor (호스트 → 장비)

| 코드 | 설명                                                 |
| ---- | ---------------------------------------------------- |
| 0x02 | Sync ACK (전원 인가 후 2초 이내 전송 시 장비 Enable) |
| 0x0C | 상태 폴링 (5초 이내 주기적으로 전송 필요)            |
| 0x0F | 에스크로 지폐 반환(Reject)                           |
| 0x10 | 에스크로 지폐 수락(Stack)                            |
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

## 테스트 클라이언트 (exe)

```bash
# 빌드 후
test_lv77_client.exe [COM포트]
# 예: test_lv77_client.exe COM4
```

- COM 포트를 9600 8E1로 열고, Sync(0x02) → Enable(0x3E) → 폴(0x0C) 루프.
- 지폐 인식 시 0x81 → 지폐코드 수신 → 0x10(수락) 전송.
