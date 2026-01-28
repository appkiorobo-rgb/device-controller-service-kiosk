# Device Controller Service 실행 가이드

## 개요

Device Controller Service는 **별도의 프로세스**로 실행되어야 합니다.
Flutter 앱은 이 서비스에 **연결만 시도**하며, 서비스를 자동으로 시작하지 않습니다.

## 실행 방법

### 1. 사전 요구사항

#### Boost 라이브러리 설치

이 프로젝트는 HTTP/WebSocket 서버를 위해 Boost.Beast를 사용합니다.

**vcpkg 사용 (권장):**
```powershell
# vcpkg 설치 (아직 없는 경우)
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg
.\bootstrap-vcpkg.bat

# Boost 설치
.\vcpkg install boost-system:x64-windows boost-thread:x64-windows
```

자세한 설치 방법은 [BOOST_INSTALLATION.md](docs/BOOST_INSTALLATION.md)를 참조하세요.

### 2. 빌드

#### vcpkg를 사용하는 경우

```powershell
cd device-controller-service-kiosk
mkdir -p out/build/x64-Release
cd out/build/x64-Release
cmake ../../.. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build . --config Release
```

#### Visual Studio에서 빌드

1. Visual Studio에서 프로젝트 열기
2. CMake 설정에서 `CMAKE_TOOLCHAIN_FILE`을 `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`로 설정 (vcpkg 사용 시)
3. Release 구성으로 빌드

### 3. 실행

빌드된 실행 파일 위치:
```
out/build/x64-Release/bin/device_controller_service.exe
```

**콘솔에서 실행:**
```bash
cd out/build/x64-Release/bin
device_controller_service.exe COM3 DEFAULT_TERM
```

인자:
- 첫 번째 인자: COM 포트 (예: COM3)
- 두 번째 인자: Terminal ID (예: DEFAULT_TERM)

**기본값:**
- COM 포트를 지정하지 않으면 COM3 사용
- Terminal ID를 지정하지 않으면 DEFAULT_TERM 사용

### 4. 실행 확인

서비스가 정상적으로 시작되면:
```
Device Controller Service is running...
Press Ctrl+C to stop.
IPC Channels:
  HTTP Server: http://localhost:8080
  WebSocket:   ws://localhost:8080/ws
  API Endpoint: POST http://localhost:8080/api/command
```

이 메시지가 보이면 서비스가 실행 중입니다.

### 5. Flutter 앱 실행

서비스가 실행 중인 상태에서 Flutter 앱을 실행:
```bash
cd ai-kiosk-client
flutter run -d windows
```

Flutter 앱에서 "서비스 연결" 버튼을 클릭하면 연결됩니다.

## 테스트 순서

1. **Device Controller Service 실행**
   ```bash
   cd device-controller-service-kiosk/out/build/x64-Release/bin
   device_controller_service.exe COM3 DEFAULT_TERM
   ```

2. **서비스 실행 확인**
   - 콘솔에 "Device Controller Service is running..." 메시지 확인

3. **Flutter 앱 실행**
   ```bash
   cd ai-kiosk-client
   flutter run -d windows
   ```

4. **테스트 화면에서 연결 테스트**
   - "서비스 연결" 버튼 클릭
   - 연결 상태가 "연결됨"으로 변경되는지 확인

## 문제 해결

### 서비스가 시작되지 않는 경우
- COM 포트가 올바른지 확인
- 다른 프로세스가 COM 포트를 사용 중인지 확인
- 로그 파일 확인: `logs/service.log`

### Flutter 앱이 연결되지 않는 경우
- 서비스가 실행 중인지 확인
- HTTP 서버가 `http://localhost:8080`에서 실행 중인지 확인
- WebSocket이 `ws://localhost:8080/ws`에서 접근 가능한지 확인
- Windows 방화벽 설정 확인 (로컬 연결이므로 일반적으로 문제 없음)

### Preview 모드로 표시되는 경우
- Windows에서 실행 중인지 확인
- Flutter 앱이 Windows 빌드인지 확인

## 향후 개선

- Windows Service로 등록하여 자동 시작
- 설정 파일로 COM 포트 및 Terminal ID 관리
- 서비스 상태 모니터링 도구
