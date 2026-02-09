; Kiosk 통합 설치 스크립트 (Inno Setup)
; - Device Controller Service (C++) + AI Kiosk Client (Flutter)
; - 설치 후 시작 프로그램 등록으로 부팅 시 자동 실행 (절대 꺼지면 안 됨)
; - 복제 방지·점주 전달 용이를 위해 단일 설치파일로 배포

#define MyAppName "AI 키오스크"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Your Company"
#define MyAppURL "https://example.com"
#define MyAppExeName "ai_kiosk_client.exe"
#define ServiceExeName "device_controller_service.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
; 설치 시 기존 파일 덮어쓰기
AllowNoIcons=yes
; 관리자 권한으로 설치 (시작 프로그램 전체 사용자용)
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=output
OutputBaseFilename=Kiosk-Setup-{#MyAppVersion}
; 아이콘 (상위 폴더 kiosk 아래 ai-kiosk-client 있을 때)
SetupIconFile=..\..\ai-kiosk-client\windows\runner\resources\app_icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; 제거 시 시작 프로그램 등록도 제거
UninstallDisplayIcon={app}\KioskClient\{#MyAppExeName}

[Languages]
; 한국어 기본 (Inno Setup 6 설치 폴더의 Languages\Korean.isl 사용)
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; 시작 프로그램 등록 (기본 체크) - 부팅 시 자동 실행
Name: "startup"; Description: "Windows 시작 시 키오스크 자동 실행 (권장)"; GroupDescription: "시작 프로그램"; Flags: checkedonce
Name: "runafter"; Description: "설치 완료 후 바로 실행"; GroupDescription: "설치 후"; Flags: unchecked

[Files]
; Device Controller Service (하드웨어 제어)
Source: "staging\DeviceControllerService\*"; DestDir: "{app}\DeviceControllerService"; Flags: ignoreversion recursesubdirs createallsubdirs
; AI Kiosk Client (Flutter UI)
Source: "staging\KioskClient\*"; DestDir: "{app}\KioskClient"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
; 시작 프로그램 등록 (서비스 먼저, 그 다음 클라이언트)
; HKEY_LOCAL_MACHINE = 모든 사용자 로그인 시 실행
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "DeviceControllerService"; ValueData: """{app}\DeviceControllerService\{#ServiceExeName}"""; Flags: uninsdeletevalue; Tasks: startup
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "AIKioskClient"; ValueData: """{app}\KioskClient\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: startup

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\KioskClient\{#MyAppExeName}"; Comment: "키오스크 UI 실행"
Name: "{group}\Device Controller Service"; Filename: "{app}\DeviceControllerService\{#ServiceExeName}"; Comment: "하드웨어 제어 서비스 (먼저 실행됨)"
Name: "{group}\키오스크 제거"; Filename: "{uninstallexe}"

[Run]
; 설치 후 선택 시 서비스 먼저 실행, 잠시 대기 후 클라이언트 실행
Filename: "{app}\DeviceControllerService\{#ServiceExeName}"; Description: "Device Controller Service 실행"; Flags: nowait postinstall skipifsilent runhidden; Tasks: runafter
Filename: "{app}\KioskClient\{#MyAppExeName}"; Description: "키오스크 클라이언트 실행"; Flags: nowait postinstall skipifsilent; Tasks: runafter

[UninstallRun]
; 제거 전 실행 중인 프로세스 종료 (선택적)
Filename: "taskkill"; Parameters: "/f /im {#MyAppExeName}"; Flags: runhidden; RunOnceId: "KillKioskClient"
Filename: "taskkill"; Parameters: "/f /im {#ServiceExeName}"; Flags: runhidden; RunOnceId: "KillService"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if WizardIsTaskSelected('startup') then
      Log('Startup registry entries added. Kiosk will run on Windows logon.');
  end;
end;
