# SMARTRO 결제 단말기 연동 프로토콜 명세서

- 제조사: SMARTRO
- 모델: TL3500 / TL3010
- 문서 버전: v3.5
- 용도: 키오스크 ↔ 결제 단말기 연동 (현금 결제 제외)

---

## 1. 통신 프로토콜 기본 사항

- 통신 방식: Serial 통신
- Byte Ordering: Little Endian
- 문자 인코딩: ASCII
- 모든 숫자 필드는 **문자열(CHAR) 기반 우측 정렬, 좌측 0 또는 space 패딩**

---

## 2. Data Type 정의

| Type | 설명 | NULL 처리 |
|----|----|----|
| CHAR | n Byte ASCII 문자열 | space 또는 0x00 |
| BYTE | 1 byte unsigned | 0x00 |
| SHORT | 2 byte signed | 0 |
| USHORT | 2 byte unsigned | 0 |
| INT | 4 byte signed | 0 |
| UINT | 4 byte unsigned | 0 |
| FLOAT | 4 byte 실수 | 0 |

---

## 3. Packet 기본 구조

### 3.1 전체 구조
STX(1) + HEADER(35) + DATA(N) + ETX(1) + BCC(1)
- Header / Tail은 고정 길이
- Data는 Job Code에 따라 가변
- BCC: STX부터 ETX까지 XOR

---

### 3.2 Header 구조 (35 Bytes)

| 항목 | 설명 | Type | Byte |
|----|----|----|----|
| STX | 0x02 | BYTE | 1 |
| Terminal ID | 좌측 정렬, 나머지 0x00 | CHAR | 16 |
| DateTime | YYYYMMDDhhmmss | CHAR | 14 |
| Job Code | 업무 코드 | CHAR | 1 |
| Response Code | 미사용 (0x00) | BYTE | 1 |
| Data Length | DATA 길이 | USHORT | 2 |

---

### 3.3 Tail 구조 (2 Bytes)

| 항목 | 설명 | Type | Byte |
|----|----|----|----|
| ETX | 0x03 | BYTE | 1 |
| BCC | STX~ETX XOR | BYTE | 1 |

---

## 4. 업무 코드 정의 (연동장치 → 결제기)

| Code | 설명 |
|----|----|
| A | 장치체크 요청 |
| B | 거래승인 요청 |
| C | 거래취소 요청 |
| D | 카드조회 요청 |
| E | 결제대기 요청 |
| F | 카드 UID 읽기 요청 |
| R | 단말기 리셋 |
| G | 부가정보 포함 거래승인 |
| H | 거래승인 응답 확인 |
| I | 설정 정보 세팅 |
| J | 설정 정보 요청 |
| K | 설정 정보 메모리 Writing |
| L | 마지막 승인 응답 요청 |
| V | 버전 체크 |
| S | 화면/음성 설정 |
| M | IC 카드 체크 |
| Q | QR / 현금영수증 승인 |

---

## 5. 업무 코드 정의 (결제기 → 연동장치)

| Code | 설명 |
|----|----|
| a | 장치체크 응답 |
| b | 거래승인 응답 |
| c | 거래취소 응답 |
| d | 카드조회 응답 |
| e | 결제대기 응답 |
| f | 카드 UID 응답 |
| @ | 이벤트 응답 |
| g | 부가정보 거래승인 응답 |
| i | 설정 정보 세팅 응답 |
| j | 설정 정보 응답 |
| k | 설정 정보 메모리 Writing 응답 |
| l | 마지막 승인 응답 |
| v | 버전 체크 응답 |
| s | 화면/음성 설정 응답 |
| m | IC 카드 체크 응답 |
| q | QR / 현금영수증 응답 |

---

## 6. 통신 Flow / ACK-NACK 규칙

### 6.1 정상 흐름

1. 요청 전문 전송
2. 수신 정상 시 ACK (0x06)
3. 응답 전문 수신
4. 응답 정상 시 ACK (0x06)

---

### 6.2 비정상 전문

- 전문 오류 시 NACK (0x15)
- 동일 전문 재전송
- 3회 실패 시 통신 실패 처리

---

### 6.3 타임아웃

- 3초 내 ACK 미수신 → 재전송
- 3회 실패 시 연결 실패

---

### 6.4 이벤트 전문

- 이벤트 전문은 ACK/NACK 미전송

| 이벤트 코드 | 설명 |
|----|----|
| @M | MS 카드 인식 |
| @R | RF 카드 인식 |
| @I | IC 카드 인식 |
| @O | IC 카드 제거 |
| @F | IC 카드 Fallback |

---

## 7. Data Format

---

### 7.1 장치체크 (A / a)

#### 요청 (A)
- **Data 없음** (Data Length = 0)
- **패킷 구조**: STX(1) + HEADER(35) + ETX(1) + BCC(1) = 총 38 bytes
  - STX: 0x02
  - Terminal ID: 16 bytes (좌측 정렬, 나머지 0x00)
  - DateTime: 14 bytes (YYYYMMDDhhmmss)
  - Job Code: 'A' (0x41)
  - Response Code: 0x00
  - Data Length: 0x00 0x00 (Little Endian)
  - ETX: 0x03
  - BCC: STX부터 ETX까지 XOR

#### 응답 (a)
- **Data Length**: 4 bytes

| 항목 | 설명 | Byte | 값 |
|----|----|----|----|
| 카드 모듈 상태 | CHAR | 1 | N (미설치) / O (정상) / X (오류) |
| RF 모듈 상태 | CHAR | 1 | O (정상) / X (오류) |
| VAN 서버 연결 | CHAR | 1 | N (미설치) / O (정상) / X (연결 디바이스 오류) / F (서버 연결 실패) |
| 연동 서버 연결 | CHAR | 1 | N (미설치) / O (정상) / X (연결 디바이스 오류) / F (서버 연결 실패) |

---

### 7.2 거래승인 요청 (B)

| 항목 | 설명 | Byte |
|----|----|----|
| 거래구분코드 | 1 승인 / 2 마지막 거래 취소 | 1 |
| 승인금액 | 원거래금액 | 10 |
| 세금 | VAT | 8 |
| 봉사료 | Service | 8 |
| 할부개월 | 00 일시불 | 2 |
| 서명여부 | 1 비서명 / 2 서명 | 1 |

Data Length = 30

---

### 7.3 거래승인 응답 (b)

> 총 Data Length = 157 Byte

포함 항목:
- 거래구분코드
- 거래매체 (IC/MS/RF/QR)
- 카드번호 (마스킹)
- 승인금액 / 세금 / 봉사료 / 할부
- 승인번호
- 매출일자 / 시간
- 거래고유번호
- 가맹점번호
- 단말기번호
- 발급사 / 매입사 코드 및 메시지
- VAN 응답 코드

---

### 7.4 거래취소 (C / c)

#### 요청 (C)

| 항목 | 설명 |
|----|----|
| 취소구분코드 | 1 요청 취소 / 2 마지막 거래 취소 |

---

## 8. 구현 필수 주의사항 (Cursor 지침)

- **BYTE 단위 길이 엄수**
- **STX / ETX / BCC 반드시 구현**
- ACK/NACK 재전송 로직 필수
- 이벤트 전문은 ACK/NACK 금지
- Data Length 값과 실제 DATA 길이 불일치 시 NACK 처리

---

## 9. 권장 아키텍처

### Windows C++ Service
- Serial 통신
- Frame Parser
- BCC 계산
- ACK/NACK State Machine
- 결제 상태 관리

### Flutter
- IPC(JSON) 기반 결제 요청
- 카드 인식 / 승인 결과 이벤트 수신
- 단말기 직접 제어 금지

---

## 10. 절대 금지 사항

- 프로토콜 구조 변경
- 필드 길이 변경
- ACK/NACK 생략
- 임의 JSON 직통 통신

본 문서는 **SMARTRO TL3500/TL3010 실기 연동을 위한 절대 기준 명세**이다.