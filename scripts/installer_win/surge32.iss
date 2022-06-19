#define MyAppPublisher "Surge Synth Team"
#define MyAppURL "https://www.surge-synth-team.org/"
#define MyAppName "Surge XT"
#define MyAppNameCondensed "SurgeXT"
#define MyAppVersion GetEnv('SURGE_VERSION')
#define MyID "69F3FE96-DEEC-4C7C-B72D-E8957EC8411C"

#if MyAppVersion == ""
#define MyAppVersion "0.0.0"
#endif

; uncomment these two lines if building the installer locally!
;#define SURGE_SRC "..\..\"
;#define SURGE_BIN "..\..\build32\Release"

[Setup]
AppId={#MyID}
AppName="{#MyAppName} {#MyAppVersion}"
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} (32-bit)
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commoncf32}\VST3\{#MyAppPublisher}\
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
DisableReadyPage=no
LicenseFile={#SURGE_SRC}\LICENSE
OutputBaseFilename=surge-xt-win32-{#MyAppVersion}-setup
SetupIconFile={#SURGE_SRC}\scripts\installer_win\surge.ico
UninstallDisplayIcon={uninstallexe}
UsePreviousAppDir=yes
Compression=lzma
SolidCompression=yes
UninstallFilesDir={commonappdata}\{#MyAppName}\uninstall
CloseApplicationsFilter=*.exe,*.vst3,*.ttf
WizardStyle=modern
; replacing that gnarly old blue computer icon in top right with... nothing!
WizardSmallImageFile={#SURGE_SRC}\scripts\installer_win\empty.bmp
WizardImageAlphaFormat=defined

[InstallDelete]
; clean up factory data folders
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\fx_presets"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\modulator_presets"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\patches_3rdparty"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\patches_factory"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\skins"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\tuning_library"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\wavetables"
Type: filesandordirs; Name: "{commonappdata}\{#MyAppName}\wavetables_3rdparty"
; clean up the mess from Surge XT 1.0 nightlies prior to PR #5727
Type: filesandordirs; Name: "{commoncf32}\VST3\{#MyAppPublisher}\Contents"
Type: filesandordirs; Name: "{commoncf32}\VST3\{#MyAppPublisher}\desktop.ini"
Type: filesandordirs; Name: "{commoncf32}\VST3\{#MyAppPublisher}\plugin.ico"
; clean up Surge XT 1.0 release stuff
Type: filesandordirs; Name: "{commonpf32}\{#MyAppPublisher}\{#MyAppName}.exe"
Type: filesandordirs; Name: "{group}\{#MyAppName}"

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Compact installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom
Name: "minimal"; Description: "Minimal installation"

[Components]
Name: CLAP; Description: {#MyAppName} CLAP (32-bit); Types: full compact custom minimal; Flags: checkablealone
Name: EffectsCLAP; Description: {#MyAppName} Effects CLAP (32-bit); Types: full custom; Flags: checkablealone
Name: VST3; Description: {#MyAppName} VST3 (32-bit); Types: full compact custom; Flags: checkablealone
Name: EffectsVST3; Description: {#MyAppName} Effects VST3 (32-bit); Types: full custom; Flags: checkablealone
Name: SA; Description: {#MyAppName} Standalone (32-bit); Types: full custom; Flags: checkablealone
Name: EffectsSA; Description: {#MyAppName} Effects Standalone (32-bit); Types: full custom; Flags: checkablealone
Name: Data; Description: Data Files; Types: full compact custom; Flags: fixed
Name: Patches; Description: Patches; Types: full compact custom; Flags: checkablealone
Name: Wavetables; Description: Wavetables; Types: full custom; Flags: checkablealone

[Files]
Source: {#SURGE_SRC}\resources\data\fx_presets\*; DestDir: {commonappdata}\{#MyAppName}\fx_presets\; Components: Data; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\modulator_presets\*; DestDir: {commonappdata}\{#MyAppName}\modulator_presets\; Components: Data; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\skins\*; DestDir: {commonappdata}\{#MyAppName}\skins\; Components: Data; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\tuning_library\*; DestDir: {commonappdata}\{#MyAppName}\tuning_library\; Components: Data; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\patches_factory\Templates\*; DestDir: {commonappdata}\{#MyAppName}\patches_factory\Templates\; Components: Data; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\patches_factory\Tutorials\*; DestDir: {commonappdata}\{#MyAppName}\patches_factory\Tutorials\; Components: Data; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\patches_factory\*; DestDir: {commonappdata}\{#MyAppName}\patches_factory\; Components: Patches; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\patches_3rdparty\*; DestDir: {commonappdata}\{#MyAppName}\patches_3rdparty\; Components: Patches; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\wavetables\*; DestDir: {commonappdata}\{#MyAppName}\wavetables\; Components: Wavetables; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\data\wavetables_3rdparty\*; DestDir: {commonappdata}\{#MyAppName}\wavetables_3rdparty\; Components: Wavetables; Flags: recursesubdirs
Source: {#SURGE_SRC}\resources\fonts\FiraMono-Regular.ttf; DestDir: "{autofonts}"; Components: Data; FontInstall: "Fira Mono"; Flags: onlyifdoesntexist uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\IndieFlower.ttf; DestDir: "{autofonts}"; Components: Data; FontInstall: "Indie Flower"; Flags: onlyifdoesntexist uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-Regular.ttf; DestDir: "{autofonts}"; Components: Data; FontInstall: "Lato"; Flags: onlyifdoesntexist uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-Bold.ttf; DestDir: "{autofonts}"; Components: Data; FontInstall: "Lato Bold"; Flags: onlyifdoesntexist uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-BoldItalic.ttf; DestDir: "{autofonts}"; Components: Data; FontInstall: "Lato Bold Italic"; Flags: onlyifdoesntexist uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-Italic.ttf; DestDir: "{autofonts}"; Components: Data; FontInstall: "Lato Italic"; Flags: onlyifdoesntexist uninsneveruninstall
Source: {#SURGE_BIN}\surge_xt_products\{#MyAppName} (32-bit).clap; DestDir: {commoncf32}\CLAP\{#MyAppPublisher}\; Components: CLAP; Flags: ignoreversion recursesubdirs
Source: {#SURGE_BIN}\surge_xt_products\{#MyAppName} Effects (32-bit).clap; DestDir: {commoncf32}\CLAP\{#MyAppPublisher}\; Components: EffectsCLAP; Flags: ignoreversion skipifsourcedoesntexist recursesubdirs
Source: {#SURGE_BIN}\surge_xt_products\{#MyAppName} (32-bit).vst3\*; DestDir: {commoncf32}\VST3\{#MyAppPublisher}\{#MyAppName} (32-bit).vst3\; Components: VST3; Flags: ignoreversion recursesubdirs
Source: {#SURGE_BIN}\surge_xt_products\{#MyAppName} Effects (32-bit).vst3\*; DestDir: {commoncf32}\VST3\{#MyAppPublisher}\{#MyAppName} Effects (32-bit).vst3\; Components: EffectsVST3; Flags: ignoreversion skipifsourcedoesntexist recursesubdirs
Source: {#SURGE_BIN}\surge_xt_products\{#MyAppName} (32-bit).exe; DestDir: {commonpf32}\{#MyAppPublisher}\{#MyAppName}\; Components: SA; Flags: ignoreversion
Source: {#SURGE_BIN}\surge_xt_products\{#MyAppName} Effects (32-bit).exe; DestDir: {commonpf32}\{#MyAppPublisher}\{#MyAppName}\; Components: EffectsSA; Flags: ignoreversion

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Icons]
Name: "{group}\{#MyAppPublisher}\{#MyAppName}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{group}\{#MyAppPublisher}\{#MyAppName}\{#MyAppName}"; Filename: "{commonpf32}\{#MyAppPublisher}\{#MyAppName}\{#MyAppName} (32-bit).exe"; WorkingDir: "{commonpf32}\{#MyAppPublisher}\{#MyAppName}"; Flags: createonlyiffileexists
Name: "{group}\{#MyAppPublisher}\{#MyAppName}\{#MyAppName} Effects"; Filename: "{commonpf32}\{#MyAppPublisher}\{#MyAppName}\{#MyAppName} Effects (32-bit).exe"; WorkingDir: "{commonpf32}\{#MyAppPublisher}\{#MyAppName}"; Flags: createonlyiffileexists

[Run]
Filename: "{cmd}"; \
    WorkingDir: "{commoncf32}\CLAP"; \
    Parameters: "/C mklink /D /J  ""{commoncf32}\CLAP\{#MyAppPublisher}\{#MyAppNameCondensed}Data"" ""{commonappdata}\{#MyAppName}"""; \
    Flags: runascurrentuser; \
    Check: WizardIsComponentSelected('CLAP') or WizardIsComponentSelected('EffectsCLAP')

Filename: "{cmd}"; \
    WorkingDir: "{commoncf32}\VST3"; \
    Parameters: "/C mklink /D /J  ""{commoncf32}\VST3\{#MyAppPublisher}\{#MyAppNameCondensed}Data"" ""{commonappdata}\{#MyAppName}"""; \
    Flags: runascurrentuser; \
    Check: WizardIsComponentSelected('VST3') or WizardIsComponentSelected('EffectsVST3')

[Code]
procedure AddToReadyMemo(var Memo: string; Info, NewLine: string);
begin
  if Info <> '' then Memo := Memo + Info + Newline + NewLine;
end;

function IsSelected(Param: String) : Boolean;
begin
  if not (Pos(Param, WizardSelectedComponents(False)) = 0) then
    Result := True
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo, MemoComponentsInfo,
                         MemoGroupInfo, MemoTasksInfo: String): String;
var
  AppData, CF, PF, AppName, AppPublisher, AppNameCondensed : String;
begin
  AddToReadyMemo(Result, MemoComponentsInfo, NewLine);

  AppData := ExpandConstant('{commonappdata}');
  CF := ExpandConstant('{commoncf32}');
  PF := ExpandConstant('{commonpf32}');
  AppName := ExpandConstant('{#MyAppName}');
  AppNameCondensed := ExpandConstant('{#MyAppNameCondensed}');
  AppPublisher := ExpandConstant('{#MyAppPublisher}');

  Result := Result + 'Installation Locations:' + NewLine
  Result := Result + Space + 'Data Files: ' + AppData + '\' + AppName + NewLine
  
  if IsSelected('clap') or IsSelected('effectsclap') then
    Result := Result + Space + 'CLAP: ' + CF + '\CLAP\' + AppPublisher + '\' + NewLine;

  if IsSelected('vst3') or IsSelected('effectsvst3') then
    Result := Result + Space + 'VST3: ' + CF + '\VST3\' + AppPublisher + '\' + NewLine;

  if IsSelected('sa') or IsSelected('effectssa') then
    Result := Result + Space + 'Standalone: ' + PF + '\' + AppPublisher + '\' + AppName + '\' + NewLine;

  if (IsSelected('clap') or IsSelected('effectsclap')) and (IsSelected('vst3') or IsSelected('effectsvst3')) then
    Result := Result + Space + 'Portable Junctions:' + NewLine
  else
    Result := Result + Space + 'Portable Junction:' + NewLine;
  
  if IsSelected('clap') or IsSelected('effectsclap') then
    Result := Result + Space + Space + 'CLAP: ' + CF + '\CLAP\' + AppPublisher + '\' + AppNameCondensed + 'Data' + NewLine;

  if IsSelected('vst3') or IsSelected('effectsvst3') then
    Result := Result + Space + Space + 'VST3: ' + CF + '\VST3\' + AppPublisher + '\' + AppNameCondensed + 'Data' + NewLine;
end;
