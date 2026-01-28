# 빠른 시작 가이드

## Visual Studio에서 CMake 프로젝트 설정 완료

### 1. CMake 도구 체인 파일 설정 ✅ (완료)
- `CMAKE_TOOLCHAIN_FILE`: `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
- 이미 올바르게 설정되어 있습니다.

### 2. 다음 단계

#### CMake 캐시 삭제 및 재구성

Visual Studio에서:

1. **솔루션 탐색기**에서 `CMakeLists.txt` 파일 찾기
2. **우클릭** → **"CMake 캐시 삭제"** 선택
3. **우클릭** → **"CMake 프로젝트 구성"** 선택

또는:

1. **프로젝트** 메뉴 → **CMake 설정** → **CMake 캐시 삭제**
2. **프로젝트** 메뉴 → **CMake 프로젝트 구성**

#### 빌드

1. **빌드** 메뉴 → **솔루션 빌드** (또는 `Ctrl+Shift+B`)
2. 또는 솔루션 탐색기에서 `device_controller_service` 우클릭 → **빌드**

### 3. 예상되는 결과

#### 성공 시:
- CMake 구성이 완료됩니다
- Boost 라이브러리를 찾았다는 메시지가 출력됩니다:
  ```
  Boost found: 1.XX.X
  Boost include dirs: ...
  Boost libraries: ...
  ```
- 빌드가 성공적으로 완료됩니다

#### 오류가 발생하는 경우:

**"No CMAKE_CXX_COMPILER could be found" 오류:**
- Visual Studio의 "Desktop development with C++" 워크로드가 설치되어 있는지 확인
- Visual Studio Installer에서 확인/설치

**"Boost not found" 오류:**
- vcpkg에서 Boost가 설치되어 있는지 확인:
  ```powershell
  C:\vcpkg\vcpkg list
  ```
- 없으면 설치:
  ```powershell
  C:\vcpkg\vcpkg install boost-system:x64-windows boost-thread:x64-windows
  ```

### 4. 빌드 출력 위치

빌드가 성공하면 실행 파일은 다음 위치에 생성됩니다:

```
out\build\x64-Debug\bin\device_controller_service.exe
```

또는

```
out\build\x64-Release\bin\device_controller_service.exe
```

### 5. 실행

빌드된 실행 파일을 실행:

```powershell
cd out\build\x64-Debug\bin
.\device_controller_service.exe COM3 DEFAULT_TERM
```

서비스가 시작되면:
```
Device Controller Service is running...
Press Ctrl+C to stop.
IPC Channels:
  HTTP Server: http://localhost:8080
  WebSocket:   ws://localhost:8080/ws
  API Endpoint: POST http://localhost:8080/api/command
```

## 문제 해결

자세한 문제 해결 방법은 다음 문서를 참조하세요:
- [VISUAL_STUDIO_CMAKE_SETUP.md](VISUAL_STUDIO_CMAKE_SETUP.md)
- [CMAKE_TROUBLESHOOTING.md](CMAKE_TROUBLESHOOTING.md)
- [BOOST_INSTALLATION.md](BOOST_INSTALLATION.md)
