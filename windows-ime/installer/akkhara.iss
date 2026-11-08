; Installer for the Akkhara Windows keyboard (Inno Setup 6).
; Built by GitHub Actions: iscc /DVersion=0.2.0 /DStage=..\stage akkhara.iss
;
; Installs to C:\Program Files\Akkhara:
;   x64\  AkkharaIME.dll + akkhara-ort.dll (ONNX Runtime) + C++ runtime   → 64-bit apps
;   x86\  the same, 32-bit                                                → 32-bit apps
;   model\ akkhara-mini (ONNX int8, 18.5 MB)
; Registers both keyboard DLLs and (optionally) adds "Akkhara (Sing Khmer)" to your keyboards.

#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Stage
  #define Stage "..\stage"
#endif

; the keyboard id: <Khmer language>:<CLSID><profile GUID> (same as src/globals.h); "{{" = a literal "{" in Inno
#define Tip "0453:{{57257CD7-B2DD-47D1-94E1-2CCC9766838A}{{0F2A00C4-EC31-49B5-AA40-89D73AE7F83B}"

[Setup]
AppId={{083E06E0-49D0-45DB-BC58-17EFD302DAD8}
AppName=Akkhara Keyboard
AppVersion={#Version}
AppPublisher=Ing Muyleang
AppPublisherURL=https://github.com/MuyleangIng/akkhara-desktop
DefaultDirName={autopf}\Akkhara
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=Akkhara-Keyboard-Setup-{#Version}
SetupIconFile=..\src\akkhara.ico
UninstallDisplayIcon={app}\x64\AkkharaIME.dll
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartIfNeededByRun=no
LicenseFile={#Stage}\LICENSE.txt

[Tasks]
Name: addkeyboard; Description: "Add Akkhara to my keyboards now (Khmer → Akkhara). Switch with Win + Space."

[Files]
; restartreplace: an app may still use the old keyboard DLL; it is replaced after a restart
Source: "{#Stage}\x64\*"; DestDir: "{app}\x64"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#Stage}\x86\*"; DestDir: "{app}\x86"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#Stage}\model\*"; DestDir: "{app}\model"; Flags: ignoreversion
Source: "{#Stage}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\add-keyboard.ps1"; DestDir: "{app}"; Flags: ignoreversion

[Run]
; register the keyboard for 64-bit and 32-bit apps
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\x64\AkkharaIME.dll"""; Flags: runhidden waituntilterminated 64bit
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/s ""{app}\x86\AkkharaIME.dll"""; Flags: runhidden waituntilterminated 32bit
; add it to the CURRENT user's Khmer keyboards (not the admin's)
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\add-keyboard.ps1"" -Tip ""{#Tip}"""; Flags: runhidden waituntilterminated runasoriginaluser; Tasks: addkeyboard

[UninstallRun]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\add-keyboard.ps1"" -Tip ""{#Tip}"" -Remove"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveKeyboard"
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\x64\AkkharaIME.dll"""; Flags: runhidden waituntilterminated 64bit; RunOnceId: "Unreg64"
Filename: "{syswow64}\regsvr32.exe"; Parameters: "/u /s ""{app}\x86\AkkharaIME.dll"""; Flags: runhidden waituntilterminated 32bit; RunOnceId: "Unreg32"

[Messages]
FinishedLabel=Akkhara is installed.%n%nPress Win + Space to switch to "Khmer – Akkhara (Sing Khmer)", then type Sing Khmer in any app: nh jg tv sala rean → Enter.%n%nApps that were already open may need to be restarted.
