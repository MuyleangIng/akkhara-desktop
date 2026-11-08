; Installer for the Akkhara Windows keyboard (Inno Setup 6).
; Built by GitHub Actions: iscc /DVersion=0.3.0 /DStage=<stage folder> akkhara.iss
;
; Installs (default C:\Program Files\Akkhara, the user can choose another folder):
;   x64\   AkkharaIME.dll + akkhara-ort.dll (ONNX Runtime) + C++ runtime   → 64-bit apps
;   x86\   the same, 32-bit                                                → 32-bit apps
;   model\ akkhara-mini (ONNX int8, 18.5 MB)
; Then registers the keyboard and adds "Akkhara (Sing Khmer)" to the user's keyboards.
; If that automatic step fails, Setup says so and opens the keyboard settings.
; Uninstall removes the keyboard from the list, unregisters it and deletes the whole folder.

#ifndef Version
  #define Version "0.0.0"
#endif
#ifndef Stage
  #define Stage "..\stage"
#endif

; the keyboard id: <Khmer language>:<CLSID><profile GUID> (same as src/globals.h); used only in [Code]
#define Tip "0453:{57257CD7-B2DD-47D1-94E1-2CCC9766838A}{0F2A00C4-EC31-49B5-AA40-89D73AE7F83B}"
#define Guide "https://github.com/MuyleangIng/akkhara-desktop#akkhara-keyboard-for-windows-10--11"

[Setup]
AppId={{083E06E0-49D0-45DB-BC58-17EFD302DAD8}
AppName=Akkhara Keyboard
AppVersion={#Version}
AppVerName=Akkhara Keyboard {#Version}
AppPublisher=Ing Muyleang
AppCopyright=© 2026 Ing Muyleang · Apache-2.0
AppPublisherURL=https://github.com/MuyleangIng/akkhara-desktop
AppSupportURL=https://github.com/MuyleangIng/akkhara-desktop/issues
VersionInfoCompany=Ing Muyleang
VersionInfoDescription=Akkhara Sing Khmer keyboard setup
DefaultDirName={autopf}\Akkhara
DisableDirPage=no
DisableWelcomePage=no
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=Akkhara-Keyboard-Setup-{#Version}
SetupIconFile=..\src\akkhara.ico
UninstallDisplayIcon={app}\akkhara.ico
UninstallDisplayName=Akkhara Keyboard
WizardStyle=modern
WizardSizePercent=110
WizardImageFile=wizard-1x.bmp,wizard-2x.bmp
WizardSmallImageFile=wizard-small-1x.bmp,wizard-small-2x.bmp
Compression=lzma2
SolidCompression=yes
CloseApplications=no
LicenseFile={#Stage}\LICENSE.txt

[Tasks]
Name: addkeyboard; Description: "Add Akkhara to my keyboards now (Khmer → Akkhara). Switch with Win + Space."
Name: shortcuts; Description: "Start menu shortcuts (guide, keyboard settings, uninstall)"

[Files]
; the keyboard DLLs: Setup registers them (regserver) and unregisters them on uninstall.
; restartreplace: an app may still use the old DLL; it is replaced after a restart.
Source: "{#Stage}\x64\AkkharaIME.dll"; DestDir: "{app}\x64"; Flags: ignoreversion regserver 64bit restartreplace uninsrestartdelete
Source: "{#Stage}\x86\AkkharaIME.dll"; DestDir: "{app}\x86"; Flags: ignoreversion regserver 32bit restartreplace uninsrestartdelete
Source: "{#Stage}\x64\*"; Excludes: "AkkharaIME.dll"; DestDir: "{app}\x64"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#Stage}\x86\*"; Excludes: "AkkharaIME.dll"; DestDir: "{app}\x86"; Flags: ignoreversion restartreplace uninsrestartdelete
Source: "{#Stage}\model\*"; DestDir: "{app}\model"; Flags: ignoreversion
Source: "{#Stage}\LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Stage}\add-keyboard.ps1"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\src\akkhara.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "howto.bmp"; Flags: dontcopy

[Icons]
Name: "{autoprograms}\Akkhara\Akkhara guide (how to type)"; Filename: "{#Guide}"; Tasks: shortcuts
Name: "{autoprograms}\Akkhara\Keyboard settings"; Filename: "{win}\explorer.exe"; Parameters: "ms-settings:regionlanguage"; IconFilename: "{app}\akkhara.ico"; Tasks: shortcuts
Name: "{autoprograms}\Akkhara\Uninstall Akkhara Keyboard"; Filename: "{uninstallexe}"; IconFilename: "{app}\akkhara.ico"; Tasks: shortcuts

[Run]
Filename: "{#Guide}"; Description: "Open the guide (how to type with Akkhara)"; Flags: postinstall shellexec nowait skipifsilent unchecked
Filename: "{win}\explorer.exe"; Parameters: "ms-settings:regionlanguage"; Description: "Open keyboard settings"; Flags: postinstall nowait skipifsilent unchecked

[UninstallDelete]
; everything left in the folder (files that were in use are removed after a restart)
Type: filesandordirs; Name: "{app}"

[Messages]
WelcomeLabel1=Welcome to Akkhara Keyboard
WelcomeLabel2=Type Sing Khmer, get Khmer (ខ្មែរ) in any app:%n%n    nh jg tv sala rean   →   ខ្ញុំចង់ទៅសាលារៀន%n%nAkkhara works offline: the AI runs on this PC and nothing you type leaves it.%n%nMade by Ing Muyleang.%n%nVersion {#Version}.
FinishedHeadingLabel=Akkhara is ready
FinishedLabelNoIcons=Press Win + Space and choose "Akkhara (Sing Khmer)", then type Sing Khmer in any app and press Enter.%n%nApps that were already open may need a restart.
FinishedLabel=Press Win + Space and choose "Akkhara (Sing Khmer)", then type Sing Khmer in any app and press Enter.%n%nApps that were already open may need a restart.

[Code]
var
  HowToPage: TWizardPage;
  KeyboardAdded: Boolean;

{ A page with a picture: how to switch (Win + Space) and type. }
procedure InitializeWizard;
var
  Img: TBitmapImage;
  Note: TNewStaticText;
begin
  HowToPage := CreateCustomPage(wpInstalling, 'How to type with Akkhara',
    'Switch with Win + Space, type Sing Khmer, press Enter.');
  ExtractTemporaryFile('howto.bmp');
  Img := TBitmapImage.Create(HowToPage);
  Img.Parent := HowToPage.Surface;
  Img.Bitmap.LoadFromFile(ExpandConstant('{tmp}\howto.bmp'));
  Img.Stretch := True;
  Img.Left := 0;
  Img.Top := 0;
  Img.Width := HowToPage.SurfaceWidth;
  Img.Height := HowToPage.SurfaceWidth * 526 div 1000;
  Note := TNewStaticText.Create(HowToPage);
  Note.Parent := HowToPage.Surface;
  Note.Top := Img.Top + Img.Height + ScaleY(8);
  Note.Width := HowToPage.SurfaceWidth;
  Note.WordWrap := True;
  Note.Caption := 'Enter = insert   ·   ↑ ↓ / Tab = choose   ·   Shift+Enter or Esc = keep the Latin   ·   Win + Space = back to English';
end;

procedure OpenKeyboardSettings;
var
  Code: Integer;
begin
  ShellExec('', ExpandConstant('{win}\explorer.exe'), 'ms-settings:regionlanguage', '', SW_SHOWNORMAL, ewNoWait, Code);
end;

{ Store apps (Mail, Start search …) can only load the keyboard if they may read its folder.
  Program Files allows that already; a folder the user chose may not, so allow it here. }
procedure AllowStoreApps;
var
  Code: Integer;
begin
  Exec(ExpandConstant('{sys}\icacls.exe'),
    '"' + ExpandConstant('{app}') + '" /grant *S-1-15-2-1:(OI)(CI)(RX) /grant *S-1-15-2-2:(OI)(CI)(RX) /T /Q',
    '', SW_HIDE, ewWaitUntilTerminated, Code);
end;

{ Add Akkhara to the keyboards of the user who started Setup (not the admin account). }
function AddKeyboard: Boolean;
var
  Code: Integer;
begin
  Result := ExecAsOriginalUser('powershell.exe',
    '-NoProfile -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}\add-keyboard.ps1') + '" -Tip "{#Tip}"',
    '', SW_HIDE, ewWaitUntilTerminated, Code) and (Code = 0);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    AllowStoreApps;
    KeyboardAdded := True;
    if WizardIsTaskSelected('addkeyboard') then
    begin
      KeyboardAdded := AddKeyboard;
      if (not KeyboardAdded) and (not WizardSilent) then
      begin
        MsgBox('Akkhara is installed, but it could not be added to your keyboards automatically.' + #13#10#13#10 +
          'Add it by hand (Settings opens now):' + #13#10 +
          '  Time & language → Language & region → Khmer → ··· → Language options →' + #13#10 +
          '  Keyboards → Add a keyboard → Akkhara (Sing Khmer).' + #13#10#13#10 +
          'No Khmer in the list? First "Add a language" → Khmer.', mbInformation, MB_OK);
        OpenKeyboardSettings;
      end;
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Code: Integer;
begin
  { before the files are deleted: take Akkhara out of the keyboard list }
  if CurUninstallStep = usUninstall then
    Exec('powershell.exe',
      '-NoProfile -ExecutionPolicy Bypass -File "' + ExpandConstant('{app}\add-keyboard.ps1') + '" -Tip "{#Tip}" -Remove',
      '', SW_HIDE, ewWaitUntilTerminated, Code);
  if (CurUninstallStep = usPostUninstall) and DirExists(ExpandConstant('{app}')) and (not UninstallSilent) then
    MsgBox('Akkhara is removed. A few files were still in use by open apps; they are deleted when you restart Windows.',
      mbInformation, MB_OK);
end;
