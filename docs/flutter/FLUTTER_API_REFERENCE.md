# Flutter API 레퍼런스

> Device Controller Service와 통신하기 위한 완전한 API 레퍼런스

## 목차

1. [개요](#개요)
2. [연결 관리](#연결-관리)
3. [명령어 API](#명령어-api)
4. [이벤트 수신](#이벤트-수신)
5. [에러 처리](#에러-처리)
6. [예제 코드](#예제-코드)

---

## 개요

### 통신 방식

- **Transport**: Windows Named Pipes (이중 채널)
- **Command/Response 파이프**: `\\.\pipe\DeviceControllerService_Command`
- **Event 파이프**: `\\.\pipe\DeviceControllerService_Event`
- **Encoding**: JSON
- **Protocol Version**: 1.0

### 통신 모델

1. **Command/Response**: 동기식 요청-응답 (Command 파이프 사용)
2. **Event Stream**: 비동기 이벤트 수신 (Event 파이프 사용)
3. **State Snapshot**: 전체 상태 조회

---

## 연결 관리

### Named Pipe 연결

Flutter에서 Windows Named Pipe를 사용하려면 플랫폼 채널을 통해 네이티브 코드를 호출해야 합니다.

#### 기본 연결 클래스

```dart
import 'dart:io';
import 'package:flutter/services.dart';

class DeviceServiceClient {
  static const MethodChannel _channel = MethodChannel('com.example/device_service');
  
  static const String commandPipeName = r'\\.\pipe\DeviceControllerService_Command';
  static const String eventPipeName = r'\\.\pipe\DeviceControllerService_Event';
  
  bool _commandConnected = false;
  bool _eventConnected = false;
  
  // Command 파이프 연결
  Future<bool> connectCommand() async {
    try {
      final result = await _channel.invokeMethod('connectPipe', {
        'pipeName': commandPipeName,
      });
      _commandConnected = result == true;
      return _commandConnected;
    } catch (e) {
      print('Failed to connect command pipe: $e');
      return false;
    }
  }
  
  // Event 파이프 연결
  Future<bool> connectEvent() async {
    try {
      final result = await _channel.invokeMethod('connectPipe', {
        'pipeName': eventPipeName,
      });
      _eventConnected = result == true;
      return _eventConnected;
    } catch (e) {
      print('Failed to connect event pipe: $e');
      return false;
    }
  }
  
  // 전체 연결
  Future<bool> connect() async {
    final cmdOk = await connectCommand();
    final evtOk = await connectEvent();
    return cmdOk && evtOk;
  }
  
  // 연결 해제
  Future<void> disconnect() async {
    await _channel.invokeMethod('disconnectPipes');
    _commandConnected = false;
    _eventConnected = false;
  }
  
  bool get isConnected => _commandConnected && _eventConnected;
}
```

---

## 명령어 API

### 공통 명령어

#### get_state_snapshot

전체 디바이스 상태 스냅샷을 조회합니다.

**요청:**
```dart
Future<Map<String, dynamic>> getStateSnapshot() async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'get_state_snapshot',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {},
  };
  
  return await _sendCommand(command);
}
```

**응답:**
```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "error": null,
  "result": {
    "deviceId1": "{\"state\":2,\"deviceType\":\"payment\",...}",
    "deviceId2": "{\"state\":2,\"deviceType\":\"printer\",...}"
  }
}
```

#### get_device_list

등록된 디바이스 목록을 조회합니다.

**요청:**
```dart
Future<Map<String, dynamic>> getDeviceList() async {
  final command = {
    'protocolVersion': '1.0',
    'kind': 'command',
    'commandId': _generateUUID(),
    'type': 'get_device_list',
    'timestampMs': DateTime.now().millisecondsSinceEpoch,
    'payload': {},
  };
  
  return await _sendCommand(command);
}
```

**응답:**
```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "payment": "payment_terminal_001",
    "printer": "printer_001",
    "camera": "camera_001"
  }
}
```

### 결제 단말기 명령어

#### payment_start

결제를 시작합니다. 결과는 이벤트로 수신됩니다.

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
      'amount': amount.toString(), // 원 단위
    },
  };
  
  return await _sendCommand(command);
}
```

**응답:**
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
    "state": "3" // PROCESSING
  }
}
```

#### payment_cancel

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

#### payment_status_check

결제 단말기 상태를 확인합니다.

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

**응답:**
```json
{
  "protocolVersion": "1.0",
  "kind": "response",
  "commandId": "uuid-here",
  "status": "OK",
  "timestampMs": 1234567890,
  "result": {
    "deviceId": "smartro_terminal_001",
    "state": "2", // 0=DISCONNECTED, 1=CONNECTING, 2=READY, 3=PROCESSING, 4=ERROR, 5=HUNG
    "stateString": "READY",
    "deviceName": "SMARTRO Payment Terminal"
  }
}
```

#### payment_reset

결제 단말기를 리셋합니다.

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

#### payment_device_check

결제 단말기 하드웨어 상태를 확인합니다.

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

## 이벤트 수신

### 이벤트 타입

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
    "amount": "10000",
    "cardNumber": "1234-****-****-5678",
    "approvalNumber": "12345678",
    "salesDate": "20240126",
    "salesTime": "123456",
    "transactionMedium": "1",
    "state": "2"
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
    "errorCode": "VAN_REJECTED",
    "errorMessage": "Insufficient funds",
    "amount": "10000",
    "state": "2"
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
    "state": "2"
  }
}
```

#### device_state_changed

디바이스 상태가 변경되었을 때 발생합니다.

```json
{
  "protocolVersion": "1.0",
  "kind": "event",
  "eventId": "uuid-here",
  "eventType": "device_state_changed",
  "timestampMs": 1234567890,
  "deviceType": "payment",
  "data": {
    "state": "3",
    "stateString": "PROCESSING"
  }
}
```

### 이벤트 수신 구현

```dart
class EventListener {
  final DeviceServiceClient _client;
  StreamSubscription? _eventSubscription;
  
  EventListener(this._client);
  
  void startListening(Function(Map<String, dynamic>) onEvent) {
    // Event 파이프에서 지속적으로 읽기
    _eventSubscription = Stream.periodic(Duration(milliseconds: 100), (_) {
      return _readEvent();
    }).listen((event) {
      if (event != null) {
        onEvent(event);
      }
    });
  }
  
  Future<Map<String, dynamic>?> _readEvent() async {
    try {
      final result = await DeviceServiceClient._channel.invokeMethod('readEvent');
      if (result != null) {
        return Map<String, dynamic>.from(jsonDecode(result));
      }
    } catch (e) {
      // 타임아웃 또는 연결 끊김
    }
    return null;
  }
  
  void stopListening() {
    _eventSubscription?.cancel();
  }
}
```

---

## 에러 처리

### 응답 에러

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

### 에러 코드

- `DEVICE_NOT_FOUND`: 디바이스가 등록되지 않음
- `DEVICE_NOT_READY`: 디바이스가 준비되지 않음
- `INVALID_PAYLOAD`: 잘못된 요청 데이터
- `COMMAND_REJECTED`: 명령어가 거부됨
- `PROCESSING_ERROR`: 처리 중 오류 발생
- `VAN_REJECTED`: VAN 서버에서 거부됨

---

## 예제 코드

### 완전한 결제 플로우 예제

```dart
import 'dart:async';
import 'dart:convert';
import 'package:uuid/uuid.dart';

class PaymentManager {
  final DeviceServiceClient _client;
  final EventListener _eventListener;
  final _uuid = Uuid();
  
  PaymentManager(this._client, this._eventListener) {
    _eventListener.startListening(_handleEvent);
  }
  
  // 결제 시작
  Future<bool> pay(int amount) async {
    try {
      // 1. 상태 확인
      final status = await checkStatus();
      if (status['result']['state'] != '2') { // READY가 아님
        print('Payment terminal is not ready');
        return false;
      }
      
      // 2. 결제 시작
      final response = await _client.sendCommand({
        'protocolVersion': '1.0',
        'kind': 'command',
        'commandId': _uuid.v4(),
        'type': 'payment_start',
        'timestampMs': DateTime.now().millisecondsSinceEpoch,
        'payload': {'amount': amount.toString()},
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
    
    return response;
  }
  
  // 이벤트 처리
  void _handleEvent(Map<String, dynamic> event) {
    if (event['kind'] != 'event' || event['deviceType'] != 'payment') {
      return;
    }
    
    final eventType = event['eventType'];
    final data = event['data'] ?? {};
    
    switch (eventType) {
      case 'payment_complete':
        _onPaymentSuccess(
          data['transactionId'],
          int.parse(data['amount']),
        );
        break;
        
      case 'payment_failed':
        _onPaymentFailure(
          data['errorCode'],
          data['errorMessage'],
        );
        break;
        
      case 'payment_cancelled':
        _onPaymentCancelled();
        break;
        
      case 'device_state_changed':
        _onStateChanged(int.parse(data['state']));
        break;
    }
  }
  
  void _onPaymentSuccess(String transactionId, int amount) {
    print('✅ Payment successful! Transaction ID: $transactionId, Amount: $amount');
  }
  
  void _onPaymentFailure(String errorCode, String errorMessage) {
    print('❌ Payment failed: $errorCode - $errorMessage');
  }
  
  void _onPaymentCancelled() {
    print('🚫 Payment cancelled');
  }
  
  void _onStateChanged(int state) {
    print('State changed: $state');
  }
}
```

---

## 주의사항

### Idempotency

모든 명령어는 `commandId`를 포함해야 하며, 동일한 `commandId`로 재전송하면 이전 응답이 반환됩니다. UUID를 사용하여 고유한 `commandId`를 생성하세요.

### 이벤트 순서

이벤트는 중복되거나 순서가 바뀔 수 있습니다. 항상 `state`를 확인하여 실제 상태를 파악하세요.

### 연결 관리

Named Pipe 연결이 끊어질 수 있으므로, 재연결 로직을 구현하세요.

### 타임아웃

모든 명령어는 타임아웃을 설정해야 합니다 (권장: 5초).
