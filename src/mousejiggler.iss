; Inno Setup script for Qt Mouse Jiggler.
;
; CI passes the version parsed out of mousejiggler.pro:
;
;     ISCC /DMyAppVersion=<version> src\mousejiggler.iss
;
; A build without a version is refused, so a hand-run must pass the parameter
; explicitly; there is no silent fallback version.
; Paths are resolved relative to this file, so the working directory
; does not matter. The installer is written to build\dist, which is gitignored.

#ifndef MyAppVersion
#error Pass the version: ISCC /DMyAppVersion=x.y.z src\mousejiggler.iss
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
; Microsoft runtime libraries next to it. But, `build/release` is also qmake's
; compilation dir (so generated `moc_*`/`qrc_*` sources end up there too), so
; the files are hand picked instead of copying the directory wholesale. The
; portable packaging step in the workflow does the same with a "staging"
; directory. Wildcards without `recursesubdirs` stay in the build directory, so
; the root `*.dll` entry covers the Qt and Microsoft runtime libraries only.
Source: "{#BuildDir}\mousejiggler.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "{#BuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion
Source: "{#BuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion
; The GPL asks that a copy of the license travels with the program, so it is
; installed as a file instead of being shown as a wizard page.
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; \
    WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Only offered for interactive installs; skipped in silent mode so package
; managers never launch the application.
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; \
    Flags: nowait postinstall skipifsilent
