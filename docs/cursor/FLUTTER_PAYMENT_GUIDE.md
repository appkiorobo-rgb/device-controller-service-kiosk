# Flutter 결제 연동 가이드

> 이 문서는 Flutter Kiosk Client에서 Device Controller Service의 결제 단말기를 사용하는 방법을 설명합니다.
> Cursor AI에게 이 문서를 제공하여 결제 기능을 구현할 수 있습니다.

---

## 1. 개요

Flutter 클라이언트는 Windows Named Pipe를 통해 Device Controller Service와 통신하여 결제 단말기를 제어합니다.

### 통신 방식

- **Transport**: Windows Named Pipe (`\\.\pipe\DeviceControllerService`)
- **Encoding**: JSON
- **Protocol Version**: 1.0

### 통신 모델

1. **Command / Response**: 동기식 요청-응답
2. **Event Stream**: 비동기 이벤트 수신 (결제 진행 상태 등)

---

## 2. 필수 설정

### 2.1 Named Pipe 연결

```dart
import 'dart:io';

class DeviceServiceClient {
  static const String pipeName = r'\\.\pipe\DeviceControllerService';
  late NamedPipeClient pipeClient;

  Future<bool> connect() async {
    try {
      pipeClient = await NamedPipeClient.connect(pipeName);
      return true;
    } catch (e) {
      print('Failed to connect to device service: $e');
      return false;
    }
  }

  void disconnect() {
    pipeClient.close();
  }
}
```

### 2.2 메시지 구조

모든 메시지는 다음 구조를 따릅니다:

```dart
class IPCMessage {
  final String protocolVersion; // "1.0"
  final String kind; // "command", "response", "event"
  final String? commandId; // UUID (command/response에만)
  final String? eventId; // UUID (event에만)
  final String? type; // command type 또는 event type
  final int timestampMs;
  final Map<String, dynamic>? payload; // command payload
  final Map<String, dynamic>? result; // response result
  final Map<String, dynamic>? error; // error object
  final String? status; // "OK", "REJECTED", "FAILED"
  final String? deviceType; // "payment" (event에만)
  final Map<String, dynamic>? data; // event data
}
```

---

## 3. 결제 명령어

### 3.1 결제 시작 (payment_start)

결제를 시작합니다. 결제는 비동기로 진행되며, 결과는 이벤트로 수신됩니다.

**요청:**

```dart
Future<Map<String, dynamic>> startPayment(int amount) async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'payment_start',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {
      'amount': amount, // 원 단위 (예: 10000 = 10,000원)
    },
  };

  final response = await _sendCommand(command);
  return response;
}
```

**응답 예시:**

```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "error": null,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": 2 // 2 = READY, 3 = PROCESSING
  }
}
```

**주의사항:**

- `amount`는 원 단위의 정수입니다 (예: 10,000원 = 10000)
- 결제 시작 후 실제 결과는 `payment_complete` 또는 `payment_failed` 이벤트로 수신됩니다
- `state`가 3 (PROCESSING)이면 결제가 진행 중입니다

---

### 3.2 결제 취소 (payment_cancel)

진행 중인 결제를 취소합니다.

**요청:**

```dart
Future<Map<String, dynamic>> cancelPayment() async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'payment_cancel',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {},
  };

  return await _sendCommand(command);
}
```

**응답 예시:**

```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": 2 // READY로 변경됨
  }
}
```

---

### 3.3 결제 상태 확인 (payment_status_check)

현재 결제 단말기의 상태를 확인합니다.

**요청:**

```dart
Future<Map<String, dynamic>> checkPaymentStatus() async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'payment_status_check',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {},
  };

  return await _sendCommand(command);
}
```

**응답 예시:**

```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": 2, // 0=DISCONNECTED, 1=CONNECTING, 2=READY, 3=PROCESSING, 4=ERROR, 5=HUNG
    "deviceName": "SMARTRO Payment Terminal"
  }
}
```

**상태 값:**

- `0`: DISCONNECTED - 단말기가 연결되지 않음
- `1`: CONNECTING - 연결 중
- `2`: READY - 결제 준비 완료
- `3`: PROCESSING - 결제 진행 중
- `4`: ERROR - 오류 발생
- `5`: HUNG - 응답 없음 (타임아웃)

---

### 3.4 단말기 리셋 (payment_reset)

결제 단말기를 리셋합니다. 오류 상태에서 복구할 때 사용합니다.

**요청:**

```dart
Future<Map<String, dynamic>> resetPaymentTerminal() async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'payment_reset',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {},
  };

  return await _sendCommand(command);
}
```

**응답 예시:**

```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": 2 // READY로 복구됨
  }
}
```

---

### 3.5 장치 체크 (payment_device_check)

결제 단말기의 하드웨어 상태를 확인합니다.

**요청:**

```dart
Future<Map<String, dynamic>> checkPaymentDevice() async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'payment_device_check',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {},
  };

  return await _sendCommand(command);
}
```

---

## 4. 이벤트 수신

결제 진행 상태는 비동기 이벤트로 수신됩니다. Named Pipe 연결 후 별도 스레드/스트림에서 이벤트를 수신해야 합니다.

### 4.1 이벤트 타입

#### payment_state_changed

결제 단말기 상태가 변경되었을 때 발생합니다.

```json
{
  "protocolVersion": "1.0",
  "kind": "event",
  "eventId": "uuid-here",
  "eventType": "payment_state_changed",
  "timestampMs": 1234567890,
  "deviceType": "payment",
  "data": {
    "state": 3 // PROCESSING
  }
}
```

#### payment_complete

결제가 성공적으로 완료되었을 때 발생합니다.

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
    "state": 2 // READY로 복귀
  }
}
```

#### payment_failed

결제가 실패했을 때 발생합니다.

```json
{
  "protocolVersion": "1.0",
  "kind": "event",
  "eventId": "uuid-here",
  "eventType": "payment_failed",
  "timestampMs": 1234567890,
  "deviceType": "payment",
  "data": {
    "errorCode": "VAN_ERROR",
    "errorMessage": "VAN response code: 01",
    "amount": 10000,
    "state": 2
  }
}
```

#### payment_cancelled

결제가 취소되었을 때 발생합니다.

```json
{
  "protocolVersion": "1.0",
  "kind": "event",
  "eventId": "uuid-here",
  "eventType": "payment_cancelled",
  "timestampMs": 1234567890,
  "deviceType": "payment",
  "data": {
    "state": 2
  }
}
```

#### payment_error

결제 단말기에서 오류가 발생했을 때 발생합니다.

```json
{
  "protocolVersion": "1.0",
  "kind": "event",
  "eventId": "uuid-here",
  "eventType": "payment_error",
  "timestampMs": 1234567890,
  "deviceType": "payment",
  "data": {
    "errorCode": "CONNECTION_FAILED",
    "errorMessage": "Failed to detect or connect to payment terminal",
    "state": 4 // ERROR
  }
}
```

### 4.2 이벤트 수신 구현 예시

```dart
class PaymentService {
  StreamSubscription? _eventSubscription;
  final _eventController = StreamController<Map<String, dynamic>>.broadcast();

  Stream<Map<String, dynamic>> get eventStream => _eventController.stream;

  void startListening() {
    // Named Pipe에서 이벤트를 지속적으로 읽는 루프
    _eventSubscription = _listenToEvents();
  }

  StreamSubscription _listenToEvents() {
    return Stream.periodic(Duration(milliseconds: 100), (_) {
      // Named Pipe에서 데이터 읽기 (논블로킹)
      // 실제 구현은 플랫폼별 채널을 사용해야 함
    }).listen((_) {
      // 이벤트 파싱 및 처리
    });
  }

  void handleEvent(Map<String, dynamic> event) {
    if (event['kind'] == 'event' && event['deviceType'] == 'payment') {
      final eventType = event['eventType'];

      switch (eventType) {
        case 'payment_complete':
          _onPaymentComplete(event['data']);
          break;
        case 'payment_failed':
          _onPaymentFailed(event['data']);
          break;
        case 'payment_cancelled':
          _onPaymentCancelled();
          break;
        case 'payment_state_changed':
          _onStateChanged(event['data']['state']);
          break;
        case 'payment_error':
          _onPaymentError(event['data']);
          break;
      }

      _eventController.add(event);
    }
  }

  void _onPaymentComplete(Map<String, dynamic> data) {
    final transactionId = data['transactionId'];
    final amount = data['amount'];
    print('Payment completed: $transactionId, Amount: $amount');
    // UI 업데이트 등
  }

  void _onPaymentFailed(Map<String, dynamic> data) {
    final errorCode = data['errorCode'];
    final errorMessage = data['errorMessage'];
    print('Payment failed: $errorCode - $errorMessage');
    // 에러 메시지 표시 등
  }

  void dispose() {
    _eventSubscription?.cancel();
    _eventController.close();
  }
}
```

---

## 5. 완전한 사용 예시

```dart
import 'dart:async';
import 'dart:convert';
import 'package:uuid/uuid.dart';

class PaymentManager {
  final DeviceServiceClient _client;
  final PaymentService _paymentService;
  final _uuid = Uuid();

  PaymentManager(this._client, this._paymentService) {
    _paymentService.eventStream.listen(_handlePaymentEvent);
  }

  // 결제 시작
  Future<bool> pay(int amount) async {
    try {
      // 1. 상태 확인
      final status = await checkStatus();
      if (status['state'] != 2) { // READY가 아님
        print('Payment terminal is not ready. State: ${status['state']}');
        return false;
      }

      // 2. 결제 시작
      final response = await _client.sendCommand({
        'protocolVersion': '1.0',
        'kind': 'command',
        'commandId': _uuid.v4(),
        'type': 'payment_start',
        'timestampMs': DateTime.now().millisecondsSinceEpoch,
        'payload': {'amount': amount},
      });

      if (response['status'] != 'OK') {
        print('Failed to start payment: ${response['error']}');
        return false;
      }

      // 3. 이벤트 대기 (payment_complete 또는 payment_failed)
      return true;
    } catch (e) {
      print('Payment error: $e');
      return false;
    }
  }

  // 결제 취소
  Future<void> cancel() async {
    await _client.sendCommand({
      'protocolVersion': '1.0',
      'kind': 'command',
      'commandId': _uuid.v4(),
      'type': 'payment_cancel',
      'timestampMs': DateTime.now().millisecondsSinceEpoch,
      'payload': {},
    });
  }

  // 상태 확인
  Future<Map<String, dynamic>> checkStatus() async {
    final response = await _client.sendCommand({
      'protocolVersion': '1.0',
      'kind': 'command',
      'commandId': _uuid.v4(),
      'type': 'payment_status_check',
      'timestampMs': DateTime.now().millisecondsSinceEpoch,
      'payload': {},
    });

    return response['result'] ?? {};
  }

  // 단말기 리셋
  Future<void> reset() async {
    await _client.sendCommand({
      'protocolVersion': '1.0',
      'kind': 'command',
      'commandId': _uuid.v4(),
      'type': 'payment_reset',
      'timestampMs': DateTime.now().millisecondsSinceEpoch,
      'payload': {},
    });
  }

  // 이벤트 처리
  void _handlePaymentEvent(Map<String, dynamic> event) {
    final eventType = event['eventType'];
    final data = event['data'] ?? {};

    switch (eventType) {
      case 'payment_complete':
        _onPaymentSuccess(data['transactionId'], data['amount']);
        break;
      case 'payment_failed':
        _onPaymentFailure(data['errorCode'], data['errorMessage']);
        break;
      case 'payment_cancelled':
        _onPaymentCancelled();
        break;
      case 'payment_state_changed':
        _onStateChanged(data['state']);
        break;
      case 'payment_error':
        _onError(data['errorCode'], data['errorMessage']);
        break;
    }
  }

  void _onPaymentSuccess(String transactionId, int amount) {
    print('✅ Payment successful! Transaction ID: $transactionId, Amount: $amount');
    // UI 업데이트: 성공 화면 표시
  }

  void _onPaymentFailure(String errorCode, String errorMessage) {
    print('❌ Payment failed: $errorCode - $errorMessage');
    // UI 업데이트: 실패 메시지 표시
  }

  void _onPaymentCancelled() {
    print('🚫 Payment cancelled');
    // UI 업데이트: 취소 메시지 표시
  }

  void _onStateChanged(int state) {
    print('State changed: $state');
    // UI 업데이트: 상태 표시
  }

  void _onError(String errorCode, String errorMessage) {
    print('⚠️ Error: $errorCode - $errorMessage');
    // UI 업데이트: 에러 메시지 표시
  }
}
```

---

## 6. 에러 처리

### 6.1 응답 에러

모든 명령어 응답에서 `status`가 `"FAILED"` 또는 `"REJECTED"`일 수 있습니다:

```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "FAILED",
  "timestampMs": 1234567890,
  "error": {
    "code": "DEVICE_NOT_READY",
    "message": "Payment terminal is not in ready state"
  },
  "result": {}
}
```

**일반적인 에러 코드:**

- `DEVICE_NOT_FOUND`: 결제 단말기가 등록되지 않음
- `DEVICE_NOT_READY`: 단말기가 준비되지 않음 (다른 상태)
- `INVALID_PAYLOAD`: 잘못된 요청 데이터
- `COMMAND_REJECTED`: 명령어가 거부됨
- `PROCESSING_ERROR`: 처리 중 오류 발생

### 6.2 타임아웃 처리

모든 명령어는 타임아웃을 설정해야 합니다:

```dart
Future<Map<String, dynamic>> sendCommandWithTimeout(
  Map<String, dynamic> command, {
  Duration timeout = const Duration(seconds: 5),
}) async {
  return await _client.sendCommand(command)
    .timeout(timeout, onTimeout: () {
      throw TimeoutException('Command timed out');
    });
}
```

### 6.3 재시도 로직

일시적인 오류에 대해서는 재시도를 고려하세요:

```dart
Future<Map<String, dynamic>> sendCommandWithRetry(
  Map<String, dynamic> command, {
  int maxRetries = 3,
}) async {
  for (int i = 0; i < maxRetries; i++) {
    try {
      final response = await sendCommandWithTimeout(command);
      if (response['status'] == 'OK') {
        return response;
      }

      // 일시적 오류가 아닌 경우 재시도하지 않음
      final errorCode = response['error']?['code'];
      if (errorCode == 'DEVICE_NOT_FOUND' ||
          errorCode == 'DEVICE_NOT_READY') {
        return response;
      }
    } catch (e) {
      if (i == maxRetries - 1) rethrow;
      await Future.delayed(Duration(milliseconds: 500 * (i + 1)));
    }
  }
  throw Exception('Max retries exceeded');
}
```

---

## 7. 주의사항

### 7.1 Idempotency

모든 명령어는 `commandId`를 포함해야 하며, 동일한 `commandId`로 재전송하면 이전 응답이 반환됩니다. UUID를 사용하여 고유한 `commandId`를 생성하세요.

### 7.2 이벤트 순서

이벤트는 중복되거나 순서가 바뀔 수 있습니다. 항상 `state`를 확인하여 실제 상태를 파악하세요.

### 7.3 상태 확인

결제 시작 전에 항상 `payment_status_check`로 단말기 상태를 확인하세요. `READY` 상태가 아니면 결제를 시작할 수 없습니다.

### 7.4 연결 관리

Named Pipe 연결이 끊어질 수 있으므로, 재연결 로직을 구현하세요:

```dart
class DeviceServiceClient {
  bool _isConnected = false;
  Timer? _reconnectTimer;

  void startAutoReconnect() {
    _reconnectTimer = Timer.periodic(Duration(seconds: 5), (_) {
      if (!_isConnected) {
        connect().then((success) {
          if (success) {
            print('Reconnected to device service');
          }
        });
      }
    });
  }

  void stopAutoReconnect() {
    _reconnectTimer?.cancel();
  }
}
```

---

## 8. 테스트 시나리오

### 시나리오 1: 정상 결제

1. `payment_status_check`로 상태 확인 → `READY` 확인
2. `payment_start`로 결제 시작
3. `payment_state_changed` 이벤트 수신 → `PROCESSING`
4. 카드 인식 대기
5. `payment_complete` 이벤트 수신 → 성공

### 시나리오 2: 결제 취소

1. `payment_start`로 결제 시작
2. 사용자가 취소 버튼 클릭
3. `payment_cancel` 명령 전송
4. `payment_cancelled` 이벤트 수신

### 시나리오 3: 결제 실패

1. `payment_start`로 결제 시작
2. 카드 인식 후 승인 실패
3. `payment_failed` 이벤트 수신 → 에러 코드 확인

### 시나리오 4: 단말기 오류 복구

1. `payment_status_check`로 상태 확인 → `ERROR` 확인
2. `payment_reset` 명령 전송
3. 상태 확인 → `READY`로 복구 확인

---

## 9. 플랫폼별 구현 참고

### Windows Named Pipe

Flutter에서 Windows Named Pipe를 사용하려면 플랫폼 채널을 사용해야 합니다:

```dart
// Method Channel을 통해 네이티브 코드 호출
static const platform = MethodChannel('com.example/device_service');

Future<Map<String, dynamic>> sendCommand(Map<String, dynamic> command) async {
  try {
    final result = await platform.invokeMethod('sendCommand', {
      'message': jsonEncode(command),
    });
    return jsonDecode(result);
  } catch (e) {
    throw Exception('Failed to send command: $e');
  }
}
```

네이티브 Windows 코드 (C# 예시):

```csharp
[DllImport("kernel32.dll", SetLastError = true)]
static extern IntPtr CreateFile(
    string lpFileName,
    uint dwDesiredAccess,
    uint dwShareMode,
    IntPtr lpSecurityAttributes,
    uint dwCreationDisposition,
    uint dwFlagsAndAttributes,
    IntPtr hTemplateFile
);

public void SendCommand(string message) {
    IntPtr pipe = CreateFile(
        @"\\.\pipe\DeviceControllerService",
        0x40000000, // GENERIC_WRITE
        0,
        IntPtr.Zero,
        3, // OPEN_EXISTING
        0,
        IntPtr.Zero
    );
    // ... 메시지 전송 로직
}
```

---

## 10. 요약 체크리스트

결제 기능 구현 시 다음을 확인하세요:

- [ ] Named Pipe 연결 구현
- [ ] UUID 생성 로직 (`commandId`용)
- [ ] `payment_start` 명령 구현
- [ ] `payment_cancel` 명령 구현
- [ ] `payment_status_check` 명령 구현
- [ ] `payment_reset` 명령 구현
- [ ] 이벤트 수신 스트림 구현
- [ ] `payment_complete` 이벤트 처리
- [ ] `payment_failed` 이벤트 처리
- [ ] `payment_cancelled` 이벤트 처리
- [ ] `payment_state_changed` 이벤트 처리
- [ ] `payment_error` 이벤트 처리
- [ ] 에러 처리 및 재시도 로직
- [ ] 타임아웃 처리
- [ ] 재연결 로직
- [ ] UI 상태 업데이트

---

이 문서를 Cursor AI에게 제공하면 Flutter에서 결제 기능을 완전히 구현할 수 있습니다.
