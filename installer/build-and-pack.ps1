# Kiosk installer build script
# 1) C++ Device Controller Service (x64-Release-Static)
# 2) Flutter AI Kiosk Client (windows release)
# 3) Copy to staging, run Inno Setup
#
# Run from device-controller-service-kiosk root: .\installer\build-and-pack.ps1
# Or from installer folder: .\build-and-pack.ps1
# Requires: Visual Studio (CMake+vcpkg), Flutter SDK, Inno Setup 6

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$StagingRoot = Join-Path $ProjectRoot 'installer\staging'
$ServiceBin = Join-Path $ProjectRoot 'out\build\x64-Release-Static\bin\Release'
$FlutterProject = Join-Path (Split-Path $ProjectRoot -Parent) 'ai-kiosk-client'
$FlutterRunner = Join-Path $FlutterProject 'build\windows\x64\runner\Release'
$InnoSetupPath = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'

function Ensure-Dir($path) {
    if (-not (Test-Path $path)) { New-Item -ItemType Directory -Path $path -Force | Out-Null }
}

function Write-Step($msg) { Write-Host (([char]10) + '== ' + $msg) -ForegroundColor Cyan }
function Write-Ok($msg)   { Write-Host ('OK: ' + $msg) -ForegroundColor Green }
function Write-Warn($msg) { Write-Host ('WARN: ' + $msg) -ForegroundColor Yellow }
function Write-Err($msg) { Write-Host ('ERROR: ' + $msg) -ForegroundColor Red }

# ---------- 1) Staging ----------
Write-Step 'Staging folder init'
$StagingService = Join-Path $StagingRoot 'DeviceControllerService'
$StagingClient = Join-Path $StagingRoot 'KioskClient'
if (Test-Path $StagingRoot) {
    Remove-Item -Path $StagingRoot -Recurse -Force
}
Ensure-Dir $StagingService
Ensure-Dir $StagingClient
Write-Ok ('Staging: ' + $StagingRoot)

# ---------- 2) C++ Device Controller Service ----------
Write-Step 'Device Controller Service build (x64-Release-Static)'
$BuildDir = Join-Path $ProjectRoot 'out\build\x64-Release-Static'
if (-not (Test-Path $BuildDir)) {
    Write-Host 'CMake config missing. Build once with x64-Release-Static in VS or run:'
    Write-Host '  cmake -B out/build/x64-Release-Static -S . -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DBUILD_FOR_DISTRIBUTION=ON'
    throw ('Build directory not found: ' + $BuildDir)
}
# Installer needs only the service exe; skip test targets (e.g. test_integrated may have build errors)
& cmake --build $BuildDir --config Release --target device_controller_service
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
Write-Ok 'Service build done'

$ServiceExe = Join-Path $ServiceBin 'device_controller_service.exe'
if (-not (Test-Path $ServiceExe)) {
    throw ('Service exe not found: ' + $ServiceExe)
}
Copy-Item -Path (Join-Path $ServiceBin '*') -Destination $StagingService -Recurse -Force
Write-Ok 'Copied Service -> staging'

# ---------- 3) Flutter ----------
Write-Step 'Flutter AI Kiosk Client build (windows release)'
if (-not (Test-Path $FlutterProject)) {
    throw ('Flutter project not found: ' + $FlutterProject + ' (expect sibling ai-kiosk-client)')
}
Push-Location $FlutterProject
try {
    flutter build windows
    if ($LASTEXITCODE -ne 0) { throw 'Flutter build failed.' }
} finally {
    Pop-Location
}
Write-Ok 'Flutter build done'

if (-not (Test-Path $FlutterRunner)) {
    throw ('Flutter runner not found: ' + $FlutterRunner)
}
Copy-Item -Path (Join-Path $FlutterRunner '*') -Destination $StagingClient -Recurse -Force
Write-Ok 'Copied KioskClient -> staging'

# ---------- 4) Inno Setup ----------
Write-Step 'Inno Setup -> installer exe'
$IssPath = Join-Path $ProjectRoot 'installer\KioskSetup.iss'
if (-not (Test-Path $InnoSetupPath)) {
    Write-Warn ('Inno Setup not found at ' + $InnoSetupPath + '. Install from https://jrsoftware.org/isinfo.php')
    Write-Host ('Staging ready at: ' + $StagingRoot)
    Write-Host 'Open KioskSetup.iss in Inno Setup and compile manually.'
    exit 0
}
& $InnoSetupPath $IssPath
if ($LASTEXITCODE -ne 0) { throw 'ISCC failed.' }

$OutDir = Join-Path $ProjectRoot 'installer\output'
Write-Ok ('Installer created in: ' + $OutDir)
$exes = Get-ChildItem -LiteralPath $OutDir | Where-Object { $_.Extension -eq '.exe' }
foreach ($e in $exes) { Write-Host ('  ' + $e.Name) }
