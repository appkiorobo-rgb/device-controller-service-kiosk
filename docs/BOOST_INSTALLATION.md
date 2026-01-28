# Boost 라이브러리 설치 가이드

이 프로젝트는 HTTP/WebSocket 서버를 위해 Boost.Beast 라이브러리를 사용합니다.

## 배포용 빌드

나중에 설치 파일로 패키징할 계획이라면, **정적 링크**를 사용해야 합니다:

```powershell
cmake -B build -S . `
    -DCMAKE_BUILD_TYPE=Release `
    -DBUILD_FOR_DISTRIBUTION=ON `
    -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

`BUILD_FOR_DISTRIBUTION=ON`을 설정하면 Boost가 정적으로 링크되어 별도의 DLL이 필요 없습니다.

자세한 내용은 [DEPLOYMENT.md](DEPLOYMENT.md)를 참조하세요.

## 방법 1: vcpkg 사용 (권장)

### 1. vcpkg 설치

```powershell
# vcpkg 클론
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg

# vcpkg 빌드
cd C:\vcpkg
.\bootstrap-vcpkg.bat
```

### 2. Boost 설치

**개발용 (동적 링크):**
```powershell
# Boost 시스템 및 스레드 라이브러리 설치 (동적 링크)
.\vcpkg install boost-system:x64-windows boost-thread:x64-windows
```

**배포용 (정적 링크):**
```powershell
# Boost 시스템 및 스레드 라이브러리 설치 (정적 링크)
.\vcpkg install boost-system:x64-windows-static boost-thread:x64-windows-static
```

**둘 다 설치 (권장):**
```powershell
# 개발용과 배포용 모두 설치
.\vcpkg install boost-system:x64-windows boost-thread:x64-windows
.\vcpkg install boost-system:x64-windows-static boost-thread:x64-windows-static
```

### 3. CMake 구성

Visual Studio에서 프로젝트를 열 때:
- CMake 설정에서 `CMAKE_TOOLCHAIN_FILE`을 `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`로 설정

또는 명령줄에서:
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

## 방법 2: 수동 설치

### 1. Boost 다운로드

1. https://www.boost.org/users/download/ 에서 최신 버전 다운로드
2. 압축 해제 (예: `C:\boost`)

### 2. 환경 변수 설정

```powershell
# 시스템 환경 변수에 추가
$env:BOOST_ROOT = "C:\boost"
```

### 3. CMake 재구성

Visual Studio에서 프로젝트를 다시 로드하거나 CMake 캐시를 삭제하고 재구성합니다.

## 방법 3: 헤더 전용 사용 (제한적)

Beast는 헤더 전용이지만, `system`과 `thread` 컴포넌트는 링크 라이브러리가 필요합니다.
이 방법은 완전한 기능을 제공하지 않을 수 있습니다.

## 확인

CMake 구성 후 다음 메시지가 표시되어야 합니다:
```
Boost found: 1.XX.X
Boost include dirs: ...
Boost libraries: ...
```

## 문제 해결

### "Boost not found" 오류

1. `BOOST_ROOT` 환경 변수가 올바르게 설정되었는지 확인
2. vcpkg를 사용하는 경우 `CMAKE_TOOLCHAIN_FILE`이 올바르게 설정되었는지 확인
3. CMake 캐시 삭제 후 재구성:
   ```powershell
   Remove-Item -Recurse -Force build
   cmake -B build -S .
   ```

### 링크 오류

Boost 라이브러리가 올바르게 링크되지 않는 경우:
1. vcpkg를 사용하는 경우: `vcpkg integrate install` 실행
2. 수동 설치의 경우: 라이브러리 경로가 올바른지 확인
