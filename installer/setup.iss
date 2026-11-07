; setup.iss - Echoes of the Abyss installer (Unicode, x64)
; Build: ISCC.exe installer\setup.iss   (or: make package)

#define MyAppName      "Echoes of the Abyss"
#define MyAppNameCN    "深渊回响"
#define MyAppVersion   "0.1.0"
#define MyAppPublisher "EchoesDev"
#define MyAppExeName   "EchoesOfTheAbyss.exe"
#define MyAppId        "{{7E4A9C2E-5F1B-4A83-9C6D-2E8B7A3F1D05}"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion=0.1.0.0
VersionInfoDescription={#MyAppName} installer
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\dist
OutputBaseFilename=EchoesOfTheAbyss-Setup-{#MyAppVersion}
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
MinVersion=10.0
; Unicode install paths supported (Inno Setup 6 is Unicode-only)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english";     MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\release\gfx\*";    DestDir: "{app}\gfx";    Flags: ignoreversion recursesubdirs
Source: "..\build\release\tables\*"; DestDir: "{app}\tables"; Flags: ignoreversion recursesubdirs
Source: "..\build\release\rooms\*";  DestDir: "{app}\rooms";  Flags: ignoreversion recursesubdirs
Source: "..\build\release\sfx\*";    DestDir: "{app}\sfx";    Flags: ignoreversion recursesubdirs
Source: "..\build\release\bgm\*";    DestDir: "{app}\bgm";    Flags: ignoreversion recursesubdirs
Source: "..\build\release\strings_zh.csv"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\release\LICENSES.txt";   DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppNameCN}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即运行 {#MyAppName}"; Flags: nowait postinstall skipifsilent
