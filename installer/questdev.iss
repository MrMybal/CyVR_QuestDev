; Copyright (c) 2026 Cyberalien
; SPDX-License-Identifier: GPL-3.0-only
; Compile through tools/build_installer.py after verifying the portable payload.
#ifndef ProductDir
  #error ProductDir is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifdef TestBuild
  #define ProductId "{{2642838C-4031-4E54-B92D-670214F669C8}"
  #define ProductName "CyVR QuestDev Installer Test"
#else
  #define ProductId "{{2642838C-4031-4E54-B92D-670214F669C7}"
  #define ProductName "CyVR QuestDev"
#endif

[Setup]
AppId={#ProductId}
AppName={#ProductName}
AppVersion={#AppVersion}
AppVerName={#ProductName} {#AppVersion}
AppPublisher=Cyberalien
AppPublisherURL=https://github.com/MrMybal/CyVR_QuestDev
AppSupportURL=https://github.com/MrMybal/CyVR_QuestDev/issues
AppUpdatesURL=https://github.com/MrMybal/CyVR_QuestDev/releases
DefaultDirName={localappdata}\Programs\{#ProductName}
DefaultGroupName={#ProductName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.10240
WizardStyle=modern
SetupIconFile={#ProductDir}\assets\branding\cyvr-questdev.ico
UninstallDisplayIcon={app}\CyVR_QuestDev.exe
UninstallDisplayName={#ProductName}
AppMutex=Local\CyVR.QuestDev.0.2
SetupMutex=Local\CyVR.QuestDev.Setup
CloseApplications=no
RestartApplications=no
DisableProgramGroupPage=yes
DisableDirPage=no
LicenseFile={#ProductDir}\LICENSE.txt
OutputDir={#OutputDir}
OutputBaseFilename=CyVR_QuestDev-{#AppVersion}-Windows-x64-Setup
Compression=lzma2/max
SolidCompression=yes
LZMANumBlockThreads=2
VersionInfoVersion={#NumericVersion}
VersionInfoDescription=CyVR QuestDev Installer
VersionInfoCopyright=Copyright (c) 2026 Cyberalien
UninstallLogMode=append

[Languages]
Name: english; MessagesFile: compiler:Default.isl
Name: french; MessagesFile: compiler:Languages\French.isl

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
Source: "{#ProductDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#ProductName}"; Filename: "{app}\CyVR_QuestDev.exe"; WorkingDir: "{app}"; AppUserModelID: "Cyberalien.CyVR.QuestDev"
Name: "{autodesktop}\{#ProductName}"; Filename: "{app}\CyVR_QuestDev.exe"; WorkingDir: "{app}"; Tasks: desktopicon; AppUserModelID: "Cyberalien.CyVR.QuestDev"

[Run]
Filename: "{app}\CyVR_QuestDev.exe"; Description: "{cm:LaunchProgram,{#ProductName}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.InvalidFolder=Choose an empty folder dedicated to CyVR QuestDev, or an existing QuestDev installation. System folders, source checkouts and links are not supported.
french.InvalidFolder=Choisissez un dossier vide dédié à CyVR QuestDev ou une installation QuestDev existante. Les dossiers système, copies de sources et liens ne sont pas pris en charge.

[Code]
function GetFileAttributes(Name: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function RegularPath(Name: String): Boolean;
var
  Attr: LongWord;
begin
  Result := False;
  while Length(Name) > 3 do begin
    Attr := GetFileAttributes(Name);
    if (Attr <> $FFFFFFFF) and ((Attr and $400) <> 0) then exit;
    Name := ExtractFileDir(Name);
  end;
  Result := True;
end;

function IsBelow(Name, Parent: String): Boolean;
begin
  Result := Pos(Lowercase(AddBackslash(Parent)), Lowercase(AddBackslash(Name))) = 1;
end;

function RegularInstallTree(Dir: String): Boolean;
var
  Find: TFindRec;
  Name: String;
begin
  Result := False;
  if not RegularPath(Dir) then exit;
  if FindFirst(AddBackslash(Dir) + '*', Find) then begin
    try
      repeat
        if (Find.Name <> '.') and (Find.Name <> '..') then begin
          if (CompareText(Find.Name, '.git') = 0) or ((Find.Attributes and $400) <> 0) then exit;
          Name := AddBackslash(Dir) + Find.Name;
          if ((Find.Attributes and $10) <> 0) and not RegularInstallTree(Name) then exit;
        end;
      until not FindNext(Find);
    finally
      FindClose(Find);
    end;
  end;
  Result := True;
end;

function ValidTarget: Boolean;
var
  Dir: String;
  Find: TFindRec;
begin
  Dir := RemoveBackslashUnlessRoot(ExpandFileName(WizardDirValue));
  Result := False;
  if (Length(Dir) <= 3) or not RegularInstallTree(Dir) then exit;
  if IsBelow(Dir, ExpandConstant('{win}')) or
     IsBelow(Dir, ExpandConstant('{commonpf}')) or
     IsBelow(Dir, ExpandConstant('{commonpf32}')) or
     IsBelow(Dir, ExpandConstant('{commonappdata}')) then exit;
  if (CompareText(Dir, GetEnv('USERPROFILE')) = 0) or
     (CompareText(Dir, ExpandConstant('{localappdata}')) = 0) or
     (CompareText(Dir, ExpandConstant('{userappdata}')) = 0) or
     (CompareText(Dir, ExpandConstant('{autodesktop}')) = 0) or
     (CompareText(Dir, ExpandConstant('{userdocs}')) = 0) then exit;
  if DirExists(AddBackslash(Dir) + '.git') or FileExists(AddBackslash(Dir) + '.git') or
     FileExists(AddBackslash(Dir) + 'CMakeLists.txt') then exit;
  if FindFirst(AddBackslash(Dir) + '*', Find) then begin
    try
      repeat
        if (Find.Name <> '.') and (Find.Name <> '..') then begin
          if not (FileExists(AddBackslash(Dir) + 'RELEASE.json') and
                  FileExists(AddBackslash(Dir) + 'UPDATE-MANIFEST.json') and
                  FileExists(AddBackslash(Dir) + 'CyVR_QuestDev.exe')) then exit;
        end;
      until not FindNext(Find);
    finally
      FindClose(Find);
    end;
  end;
  Result := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectDir then begin
    Result := ValidTarget;
    if not Result then SuppressibleMsgBox(CustomMessage('InvalidFolder'), mbError, MB_OK, IDOK);
  end;
end;

function SafeManagedName(Name: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  if (Name = '') or (Length(Name) > 240) then exit;
  if (Name[1] = '/') or (Name[1] = '\') or (Pos('..', Name) > 0) then exit;
  for I := 1 to Length(Name) do
    if (Ord(Name[I]) < 32) or (Pos(Name[I], ':*?"<>|\') > 0) then exit;
  Result := True;
end;

procedure RemovePythonCache(Name: String);
var
  Cache, Base: String;
  Find: TFindRec;
begin
  if CompareText(Copy(Name, Length(Name) - 2, 3), '.py') <> 0 then exit;
  Cache := AddBackslash(ExtractFileDir(Name)) + '__pycache__';
  if not RegularPath(Cache) then exit;
  Base := ExtractFileName(Name);
  Base := Copy(Base, 1, Length(Base) - 3);
  if FindFirst(AddBackslash(Cache) + Base + '.*.pyc', Find) then begin
    try
      repeat
        if (Find.Attributes and ($10 or $400)) = 0 then
          DeleteFile(AddBackslash(Cache) + Find.Name);
      until not FindNext(Find);
    finally
      FindClose(Find);
    end;
  end;
  RemoveDir(Cache);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Files: TArrayOfString;
  I: Integer;
  Root, Name, Dir: String;
begin
  if CurUninstallStep <> usUninstall then exit;
  Root := RemoveBackslashUnlessRoot(ExpandConstant('{app}'));
  if not RegularPath(Root) then exit;
  if not LoadStringsFromFile(AddBackslash(Root) + 'INSTALL-FILES.txt', Files) then exit;
  if GetArrayLength(Files) > 16000 then exit;
  { Validate the complete inventory before removing any managed file. }
  for I := 0 to GetArrayLength(Files) - 1 do
    if not SafeManagedName(Files[I]) then exit;
  for I := 0 to GetArrayLength(Files) - 1 do begin
    Name := Files[I];
    StringChangeEx(Name, '/', '\', True);
    Name := AddBackslash(Root) + Name;
    if RegularPath(Name) then begin
      RemovePythonCache(Name);
      DeleteFile(Name);
      Dir := ExtractFileDir(Name);
      while (CompareText(Dir, Root) <> 0) and IsBelow(Dir, Root) do begin
        RemoveDir(Dir); { Empty directories only; never recurse into user data. }
        Dir := ExtractFileDir(Dir);
      end;
    end;
  end;
end;
