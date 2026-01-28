# 배포 가이드 (Deployment Guide)

이 문서는 Device Controller Service를 설치 프로그램으로 패키징하기 위한 가이드입니다.

## 개요

프로젝트는 두 가지 빌드 모드를 지원합니다:
- **개발용 (동적 링크)**: 개발 중 사용, Boost DLL 필요
- **배포용 (정적 링크)**: 설치 프로그램용, DLL 없이 단일 실행 파일

## 사전 준비

### 1. Boost 라이브러리 설치

PowerShell을 **관리자 권한**으로 실행하고 다음 스크립트를 실행하세요:

```powershell
.\install_boost.ps1
```

또는 수동으로:

```powershell
# 개발용 (동적 링크)
C:\vcpkg\vcpkg install boost-beast:x64-windows boost-asio:x64-windows boost-system:x64-windows boost-thread:x64-windows

# 배포용 (정적 링크)
C:\vcpkg\vcpkg install boost-beast:x64-windows-static boost-asio:x64-windows-static boost-system:x64-windows-static boost-thread:x64-windows-static
```

## 빌드 구성

### Visual Studio에서 빌드

1. **개발용 빌드**:
   - 구성 선택: `x64-Debug` 또는 `x64-Release`
   - Boost DLL이 필요합니다

2. **배포용 빌드** (정적 링크):
   - 구성 선택: `x64-Release-Static`
   - 단일 실행 파일 생성 (DLL 불필요)

### 명령줄에서 빌드

```powershell
# 개발용 Release 빌드
cmake -B build -S . `
    -G "Visual Studio 18 2026 Win64" `
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release

# 배포용 정적 링크 빌드
cmake -B build-static -S . `
    -G "Visual Studio 18 2026 Win64" `
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-windows-static `
    -DBUILD_FOR_DISTRIBUTION=ON `
    -DCMAKE_BUILD_TYPE=Release

cmake --build build-static --config Release
```

## 배포 파일 구조

배포용 빌드 후 다음 파일들이 생성됩니다:

```
out/build/x64-Release-Static/bin/
  └── device_controller_service.exe  (단일 실행 파일, DLL 불필요)
```

## 설치 프로그램 제작

### WiX Toolset 사용 예시

```xml
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="*" Name="Device Controller Service" Language="1033" Version="1.0.0.0" 
           Manufacturer="Your Company" UpgradeCode="YOUR-GUID-HERE">
    <Package InstallerVersion="200" Compressed="yes" InstallScope="perMachine" />
    
    <MajorUpgrade DowngradeErrorMessage="A newer version is already installed." />
    
    <MediaTemplate />
    
    <Feature Id="ProductFeature" Title="Device Controller Service" Level="1">
      <ComponentRef Id="ServiceExe" />
    </Feature>
    
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder">
        <Directory Id="INSTALLFOLDER" Name="DeviceControllerService">
          <Component Id="ServiceExe" Guid="YOUR-GUID-HERE">
            <File Id="ServiceExe" Source="out\build\x64-Release-Static\bin\device_controller_service.exe" 
                  KeyPath="yes" />
          </Component>
        </Directory>
      </Directory>
    </Directory>
  </Product>
</Wix>
```

### Inno Setup 사용 예시

```pascal
[Setup]
AppName=Device Controller Service
AppVersion=1.0.0
DefaultDirName={pf}\DeviceControllerService
DefaultGroupName=Device Controller Service
OutputDir=installer
OutputBaseFilename=DeviceControllerService-Setup

[Files]
Source: "out\build\x64-Release-Static\bin\device_controller_service.exe"; \
    DestDir: "{app}"; Flags: ignoreversion

[Run]
Filename: "{app}\device_controller_service.exe"; \
    Description: "Start Device Controller Service"; \
    Flags: nowait postinstall skipifsilent
```

## 서비스 등록 (선택사항)

Windows 서비스로 등록하려면:

```powershell
# 서비스 등록
sc.exe create DeviceControllerService binPath= "C:\Program Files\DeviceControllerService\device_controller_service.exe" start= auto

# 서비스 시작
sc.exe start DeviceControllerService

# 서비스 중지
sc.exe stop DeviceControllerService

# 서비스 제거
sc.exe delete DeviceControllerService
```

## 의존성 확인

### 정적 링크 빌드 확인

배포용 빌드가 제대로 정적 링크되었는지 확인:

```powershell
# Dependency Walker 또는 dumpbin 사용
dumpbin /dependents out\build\x64-Release-Static\bin\device_controller_service.exe
```

출력에 `boost_*.dll`이 없어야 합니다. Windows 시스템 DLL만 있어야 합니다:
- `KERNEL32.DLL`
- `MSVCP*.DLL` (Visual C++ 런타임)
- `VCRUNTIME*.DLL`

### Visual C++ 런타임

정적 링크 빌드도 Visual C++ 런타임은 필요할 수 있습니다. 다음 중 하나를 포함하세요:

1. **Visual C++ 재배포 가능 패키지** 설치
2. **설치 프로그램에 vcredist 포함**

## 문제 해결

### "DLL을 찾을 수 없습니다" 오류

- 정적 링크 빌드를 사용했는지 확인
- `x64-Release-Static` 구성으로 빌드했는지 확인
- `dumpbin /dependents`로 의존성 확인

### Boost 헤더를 찾을 수 없음

- `install_boost.ps1` 스크립트 실행 확인
- CMake 캐시 삭제 후 재구성
- vcpkg 경로 확인 (`C:\vcpkg\scripts\buildsystems\vcpkg.cmake`)

## 참고 자료

- [vcpkg 문서](https://github.com/Microsoft/vcpkg)
- [WiX Toolset](https://wixtoolset.org/)
- [Inno Setup](https://jrsoftware.org/isinfo.php)
- [Boost 라이브러리 문서](https://www.boost.org/doc/)
