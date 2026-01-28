# Boost 설치 스크립트 (vcpkg 사용)
# 정적/동적 링크 모두 지원
# PowerShell을 관리자 권한으로 실행하세요

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Boost 라이브러리 설치 스크립트" -ForegroundColor Cyan
Write-Host "정적 배포 및 개발용 모두 설치" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# vcpkg 경로 확인
$vcpkgPath = "C:\vcpkg\vcpkg.exe"
if (-not (Test-Path $vcpkgPath)) {
    Write-Host "오류: vcpkg를 찾을 수 없습니다. C:\vcpkg\vcpkg.exe 경로를 확인하세요." -ForegroundColor Red
    Write-Host "vcpkg 설치 방법:" -ForegroundColor Yellow
    Write-Host "  git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg" -ForegroundColor Yellow
    Write-Host "  cd C:\vcpkg" -ForegroundColor Yellow
    Write-Host "  .\bootstrap-vcpkg.bat" -ForegroundColor Yellow
    exit 1
}

Write-Host "vcpkg 경로: $vcpkgPath" -ForegroundColor Green
Write-Host ""

# 1. 개발용 (동적 링크) - x64-windows
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "1단계: 개발용 Boost 설치 (동적 링크)" -ForegroundColor Yellow
Write-Host "트리플릿: x64-windows" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host ""

$packages = @(
    "boost-beast:x64-windows",
    "boost-asio:x64-windows",
    "boost-system:x64-windows",
    "boost-thread:x64-windows"
)

foreach ($package in $packages) {
    Write-Host "설치 중: $package" -ForegroundColor Cyan
    & $vcpkgPath install $package
    if ($LASTEXITCODE -ne 0) {
        Write-Host "오류: $package 설치 실패" -ForegroundColor Red
        exit 1
    }
    Write-Host ""
}

# 2. 배포용 (정적 링크) - x64-windows-static
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "2단계: 배포용 Boost 설치 (정적 링크)" -ForegroundColor Yellow
Write-Host "트리플릿: x64-windows-static" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host ""

$staticPackages = @(
    "boost-beast:x64-windows-static",
    "boost-asio:x64-windows-static",
    "boost-system:x64-windows-static",
    "boost-thread:x64-windows-static"
)

foreach ($package in $staticPackages) {
    Write-Host "설치 중: $package" -ForegroundColor Cyan
    & $vcpkgPath install $package
    if ($LASTEXITCODE -ne 0) {
        Write-Host "오류: $package 설치 실패" -ForegroundColor Red
        exit 1
    }
    Write-Host ""
}

# 완료 메시지
Write-Host "========================================" -ForegroundColor Green
Write-Host "설치 완료!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "설치된 패키지:" -ForegroundColor Yellow
Write-Host "  - 개발용 (동적): x64-windows" -ForegroundColor Cyan
Write-Host "  - 배포용 (정적): x64-windows-static" -ForegroundColor Cyan
Write-Host ""
Write-Host "다음 단계:" -ForegroundColor Yellow
Write-Host "  1. Visual Studio에서 프로젝트 열기" -ForegroundColor White
Write-Host "  2. 프로젝트 → CMake 캐시 삭제" -ForegroundColor White
Write-Host "  3. CMake 자동 재구성 대기" -ForegroundColor White
Write-Host "  4. 빌드 시도" -ForegroundColor White
Write-Host ""
Write-Host "배포용 빌드:" -ForegroundColor Yellow
Write-Host "  - CMakeSettings.json에서 'x64-Release-Static' 구성 선택" -ForegroundColor White
Write-Host "  - 또는 BUILD_FOR_DISTRIBUTION=ON으로 빌드" -ForegroundColor White
