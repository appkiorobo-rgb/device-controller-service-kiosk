# 설치 없이 exe만으로 실제 키오스크에서 테스트하기

설치 프로그램 없이 **device-controller-service**와 **ai-kiosk-client**를 exe로만 빌드해 실제 키오스크 PC에서 실행하는 방법입니다.

---

## 0. 한 번에 빌드 (스크립트)

kiosk 루트(`device-controller-service-kiosk`와 `ai-kiosk-client`의 상위 폴더)에서:

```powershell
# 두 프로젝트 모두 빌드
.\build-and-run-exe.ps1

# 빌드 후 키오스크에 복사할 폴더까지 한 번에 묶기 (KioskTest\DeviceController, KioskTest\AiKioskClient)
.\build-and-run-exe.ps1 -Pack
```

**스크립트 실행이 안 될 때** (실행 정책 오류):  
한 번만 실행하려면 `powershell -ExecutionPolicy Bypass -File .\build-and-run-exe.ps1` 로 실행하거나,  
`Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser` 로 현재 사용자에 한해 허용한 뒤 `.\build-and-run-exe.ps1` 을 사용하세요.

- **Visual Studio 2022**와 **CMake**, **Flutter**가 PATH에 있어야 합니다.
- VS 버전이 다르면 스크립트 안의 `-G "Visual Studio 17 2022"`를 본인 환경에 맞게 수정하세요.

---

## 1. 사전 준비

- **Visual Studio** (C++ 데스크톱 개발, CMake)
  - device-controller 빌드용
- **Flutter SDK** (Windows)
  - ai-kiosk-client 빌드용
- **EDSDK**
  - `device-controller-service-kiosk/edsdk/Dll.zip` 압축 해제 → `edsdk/Dll/` 폴더에 `*.dll` 있어야 빌드·실행 가능

---

## 2. device-controller-service 빌드

### 방법 A: Visual Studio에서

1. `device-controller-service-kiosk` 폴더를 Visual Studio에서 **열기** (CMake 프로젝트로 열기).
2. 상단 구성에서 **x64-Release** 선택.
3. **빌드** → **솔루션 빌드** (또는 `device_controller_service` 우클릭 → 빌드).

출력 위치:

```
device-controller-service-kiosk\out\build\x64-Release\bin\device_controller_service.exe
```

같은 `bin` 폴더에 EDSDK용 `*.dll`이 복사되어 있어야 합니다. (CMake POST_BUILD에서 복사)

### 방법 B: 명령줄(CMake + MSBuild)

```powershell
cd c:\Users\kioro\Desktop\kiosk\device-controller-service-kiosk

mkdir out\build\x64-Release -Force
cd out\build\x64-Release
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..\..\..
cmake --build . --config Release
cd ..\..\..
```

실행 파일 경로:

```
out\build\x64-Release\bin\device_controller_service.exe
```

- Visual Studio 2022가 없으면 `Visual Studio 16 2019` 등 본인 버전에 맞게 `-G` 값만 바꾸면 됩니다.

---

## 3. ai-kiosk-client (Flutter) 빌드

```powershell
cd c:\Users\kioro\Desktop\kiosk\ai-kiosk-client
flutter pub get
flutter build windows
```

출력 위치 (Flutter 3.16+):

```
ai-kiosk-client\build\windows\x64\runner\Release\
```

또는 (이전 Flutter):

```
ai-kiosk-client\build\windows\runner\Release\
```

이 폴더 안에 **전부** 필요합니다.

- `ai_kiosk_client.exe`
- `flutter_windows.dll`
- `data/` 폴더
- 기타 `*.dll` 등

---

## 4. 실제 키오스크에서 실행하는 방법

### 4-1. 복사할 것

1. **서비스(device-controller)**
   - `device-controller-service-kiosk\out\build\x64-Release\bin\` **폴더 전체**를 키오스크로 복사
   - 예: `D:\KioskTest\DeviceController\`
   - 내용: `device_controller_service.exe` + `*.dll` (EDSDK 등)

2. **클라이언트(Flutter)**
   - `ai-kiosk-client\build\windows\x64\runner\Release\` **폴더 전체**를 키오스크로 복사
   - 예: `D:\KioskTest\AiKioskClient\`
   - 내용: `ai_kiosk_client.exe` + `flutter_windows.dll` + `data\` + 기타 dll

3. **설정(선택)**
   - 서비스는 exe와 같은 폴더의 `config.ini`를 읽습니다.
   - 키오스크에서 이미 쓰던 `config.ini`가 있으면 **서비스 exe와 같은 폴더**에 넣으면 됩니다.
   - 없으면 서비스가 기본값으로 동작하고, 필요 시 Admin에서 프린터/결제 COM 포트 등 설정 후 자동으로 저장됩니다.

### 4-2. 실행 순서

1. **먼저 서비스 실행**
   - `DeviceController\device_controller_service.exe` 실행
   - 콘솔에 `Named pipe created...` / `Device Controller Service is running...` 나오면 대기 상태.

2. **그 다음 클라이언트 실행**
   - `AiKioskClient\ai_kiosk_client.exe` 실행
   - Named Pipe `\\.\pipe\DeviceControllerService`로 서비스에 연결합니다.

- 서비스를 끄면 클라이언트는 “서비스 연결” 실패가 됩니다.
- 테스트 후에는 **서비스 먼저 종료**(Ctrl+C 등), 그 다음 클라이언트 종료하면 됩니다.

### 4-3. 한 폴더로 묶어서 배포하고 싶을 때

예시 구조:

```
D:\KioskTest\
  DeviceController\
    device_controller_service.exe
    (EDSDK 등 *.dll)
    config.ini          ← 있으면 넣기
  AiKioskClient\
    ai_kiosk_client.exe
    flutter_windows.dll
    data\
    (기타 dll)
```

- 서비스와 클라이언트는 **각각 자신 폴더**에서 실행하면 됩니다 (상대경로/데이터 경로가 exe 기준이므로).

---

## 5. 요약

| 항목           | 경로/명령                                                                                 |
| -------------- | ----------------------------------------------------------------------------------------- |
| 서비스 exe     | `device-controller-service-kiosk\out\build\x64-Release\bin\device_controller_service.exe` |
| 클라이언트 exe | `ai-kiosk-client\build\windows\x64\runner\Release\ai_kiosk_client.exe`                    |
| 실행 순서      | 1) device_controller_service.exe → 2) ai_kiosk_client.exe                                 |
| 설정           | 서비스 exe와 같은 폴더에 `config.ini` (없으면 기본값)                                     |

설치(인스톨러) 없이 위처럼 exe와 dll, data만 복사해 두고 실행하면 실제 키오스크에서 동일하게 테스트할 수 있습니다.
