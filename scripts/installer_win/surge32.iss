#define MyAppPublisher "Surge Synth Team"
#define MyAppURL "http://www.surge-synth-team.org/"
#define MyAppName "Surge XT"
#define MyAppVersion GetEnv('SURGE_VERSION')
#define MyID "69F3FE96-DEEC-4C7C-B72D-E8957EC8411C"

#if MyAppVersion == ""
#define MyAppVersion "0.0.0"
#endif

[Setup]
AppId={#MyID}
AppName="{#MyAppName} {#MyAppVersion}"
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} (32-bit)
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commoncf32}\VST3\Surge Synth Team\
DefaultGroupName=Surge XT
DisableProgramGroupPage=yes
DisableDirPage=yes
DisableReadyPage=no
LicenseFile={#SURGE_SRC}\LICENSE
OutputBaseFilename="surge-xt-win32-{#MyAppVersion}-setup"
SetupIconFile={#SURGE_SRC}\scripts\installer_win\surge.ico
UninstallDisplayIcon={#SURGE_SRC}\scripts\installer_win\surge.ico
UsePreviousAppDir=yes
Compression=lzma
SolidCompression=yes
UninstallFilesDir={commonappdata}\Surge XT\uninstall\x86\

[InstallDelete]
;; clean up factory data folder, except tuning_library folder (users might link to their own custom tunings into this folder)
Type: filesandordirs; Name: "{commonappdata}\Surge XT\fx_presets"
Type: filesandordirs; Name: "{commonappdata}\Surge XT\modulator_presets"
Type: filesandordirs; Name: "{commonappdata}\Surge XT\patches_3rdparty"
Type: filesandordirs; Name: "{commonappdata}\Surge XT\patches_factory"
Type: filesandordirs; Name: "{commonappdata}\Surge XT\skins"
Type: filesandordirs; Name: "{commonappdata}\Surge XT\wavetables"
Type: filesandordirs; Name: "{commonappdata}\Surge XT\wavetables_3rdparty"

[Components]
Name: VST3; Description: Surge XT VST3 (32-bit); Types: full compact custom; Flags: checkablealone
Name: EffectsVST3; Description: Surge XT Effects VST3 (32-bit); Types: full custom; Flags: checkablealone
Name: SA; Description: Surge XT Standalone (32-bit); Types: full custom; Flags: checkablealone
Name: EffectsSA; Description: Surge XT Effects Standalone (32-bit); Types: full custom; Flags: checkablealone
Name: Data; Description: Data Files; Types: full compact custom; Flags: fixed

[Files]
Source: {#SURGE_SRC}\resources\data\*; DestDir: {commonappdata}\Surge XT\; Components: Data; Flags: recursesubdirs; Excludes: "*.git,windows.wt,configuration.xml,paramdocumentation.xml";
Source: {#SURGE_SRC}\resources\fonts\FiraMono-Regular.ttf; DestDir: "{fonts}"; Components: Data; FontInstall: "Fira Mono"; Flags: uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\IndieFlower.ttf; DestDir: "{fonts}"; Components: Data; FontInstall: "Indie Flower"; Flags: uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-Regular.ttf; DestDir: "{fonts}"; Components: Data; FontInstall: "Lato"; Flags: uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-Bold.ttf; DestDir: "{fonts}"; Components: Data; FontInstall: "Lato Bold"; Flags: uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-BoldItalic.ttf; DestDir: "{fonts}"; Components: Data; FontInstall: "Lato Bold Italic"; Flags: uninsneveruninstall
Source: {#SURGE_SRC}\resources\fonts\Lato-Italic.ttf; DestDir: "{fonts}"; Components: Data; FontInstall: "Lato Italic"; Flags: uninsneveruninstall
Source: {#SURGE_BIN}\surge_xt_products\Surge XT (32-bit).vst3\*; DestDir: {commoncf32}\VST3\Surge Synth Team\Surge XT (32-bit).vst3\; Components: VST3; Flags: ignoreversion recursesubdirs
Source: {#SURGE_BIN}\surge_xt_products\Surge XT Effects (32-bit).vst3\*; DestDir: {commoncf32}\VST3\Surge Synth Team\Surge XT Effects (32-bit).vst3\; Components: EffectsVST3; Flags: ignoreversion skipifsourcedoesntexist recursesubdirs
Source: {#SURGE_BIN}\surge_xt_products\Surge XT (32-bit).exe; DestDir: {commonpf32}\Surge Synth Team\; Components: SA; Flags: ignoreversion
Source: {#SURGE_BIN}\surge_xt_products\Surge XT Effects (32-bit).exe; DestDir: {commonpf32}\Surge Synth Team\; Components: EffectsSA; Flags: ignoreversion

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Icons]
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Run]
Filename: "{cmd}"; \
    WorkingDir: "{commoncf32}\VST3"; \
    Parameters: "/C mklink /D /J  ""{commoncf32}\VST3\Surge Synth Team\SurgeXTData"" ""{commonappdata}\Surge XT"""; \
    Flags: runascurrentuser

[Code]
procedure AddToReadyMemo(var Memo: string; Info, NewLine: string);
begin
  if Info <> '' then Memo := Memo + Info + Newline + NewLine;
end;

function UpdateReadyMemo(
  Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo, MemoComponentsInfo,
  MemoGroupInfo, MemoTasksInfo: String): String;
begin
  AddToReadyMemo(Result, MemoComponentsInfo, NewLine);

  Result := Result + 'Installation Locations:' + NewLine
  Result := Result + Space + 'Data Files: ' + ExpandConstant( '{commonappdata}' ) + '\Surge XT' + NewLine
  Result := Result + Space + 'VST3 Plugins: ' + ExpandConstant( '{commoncf32}' ) + '\VST3\Surge Synth Team' + NewLine
  Result := Result + Space + 'Standalone: ' + ExpandConstant( '{commonpf32}' ) + '\Surge Synth Team' + NewLine
  Result := Result + Space + 'Portable Junction: ' + ExpandConstant( '{commoncf32}' ) + '\VST3\Surge Synth Team\SurgeXTData' + NewLine
  
end;
