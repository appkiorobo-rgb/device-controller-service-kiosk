# Flutter 빠른 시작 가이드

> Device Controller Service를 Flutter에서 빠르게 시작하는 가이드

## 1. 설정

### 1.1 플랫폼 채널 설정

`android/app/src/main/kotlin/.../MainActivity.kt` 또는 Windows 네이티브 코드에 Named Pipe 연결 코드를 추가합니다.

### 1.2 의존성 추가

```yaml
dependencies:
  uuid: ^3.0.0
```

## 2. 기본 사용법

### 2.1 연결

```dart
final client = DeviceServiceClient();
final connected = await client.connect();
if (!connected) {
  print('Failed to connect to device service');
  return;
}
```

### 2.2 결제 시작

```dart
final paymentManager = PaymentManager(client, eventListener);

// 결제 시작 (10,000원)
final success = await paymentManager.pay(10000);
if (success) {
  print('Payment started, waiting for result...');
}
```

### 2.3 이벤트 수신

```dart
final eventListener = EventListener(client);
eventListener.startListening((event) {
  if (event['eventType'] == 'payment_complete') {
    print('Payment completed!');
  }
});
```

## 3. 완전한 예제

```dart
import 'package:flutter/material.dart';
import 'device_service_client.dart';
import 'payment_manager.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  DeviceServiceClient? _client;
  PaymentManager? _paymentManager;
  String _status = 'Not connected';
  
  @override
  void initState() {
    super.initState();
    _initialize();
  }
  
  Future<void> _initialize() async {
    _client = DeviceServiceClient();
    final connected = await _client!.connect();
    
    if (connected) {
      setState(() {
        _status = 'Connected';
      });
      
      final eventListener = EventListener(_client!);
      _paymentManager = PaymentManager(_client!, eventListener);
    } else {
      setState(() {
        _status = 'Connection failed';
      });
    }
  }
  
  Future<void> _startPayment() async {
    if (_paymentManager == null) return;
    
    setState(() {
      _status = 'Starting payment...';
    });
    
    final success = await _paymentManager!.pay(10000);
    if (!success) {
      setState(() {
        _status = 'Failed to start payment';
      });
    }
  }
  
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(title: Text('Payment Example')),
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text('Status: $_status'),
              SizedBox(height: 20),
              ElevatedButton(
                onPressed: _startPayment,
                child: Text('Pay 10,000 KRW'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
```

## 4. 다음 단계

- [완전한 API 레퍼런스](FLUTTER_API_REFERENCE.md) 참조
- [예제 코드 모음](FLUTTER_EXAMPLES.md) 참조
