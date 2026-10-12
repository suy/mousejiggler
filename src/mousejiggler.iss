; Inno Setup script for Qt Mouse Jiggler.
;
; CI passes the version parsed out of mousejiggler.pro:
;
;     ISCC /DMyAppVersion=0.1.0 src\mousejiggler.iss
;
; Running it by hand from the repository root works too; the fallback version
; gets a -dev suffix so a local build cannot be confused with a released one.
; Paths are resolved relative to this file, so the working directory
; does not matter. The installer is written to build\dist, which is gitignored.

#ifndef MyAppVersion
#define MyAppVersion "0.1.0-dev"
#endif

#define MyAppName "Qt Mouse Jiggler"
#define MyAppExeName "mousejiggler.exe"
#define MyAppPublisher "Alejandro Exojo"
#define MyAppURL "https://github.com/suy/mousejiggler"
#define BuildDir "..\build\release"

[Setup]
; Stable identifier for the application; keep constant across versions so an
; upgrade replaces an existing install instead of installing side by side.
AppId={{6F5C9A21-4B7D-4E3A-9C58-2E1D0F8B4A37}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
; Per-user install: under the user profile, no UAC prompt, no PATH changes.
; The application is started from the Start Menu, not from a console.
PrivilegesRequired=lowest
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=mousejiggler.ico
LicenseFile=..\LICENSE
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupLogging=yes
OutputDir=..\build\dist
OutputBaseFilename=MouseJiggler-{#MyAppVersion}-x64-setup

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; windeployqt output: the executable with the Qt runtime, Qt plugins and the
; Microsoft runtime libraries next to it.
Source: "{#BuildDir}\*"; DestDir: "{app}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; \
    WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Only offered for interactive installs; skipped in silent mode so package
; managers never launch the application.
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; \
    Flags: nowait postinstall skipifsilent
