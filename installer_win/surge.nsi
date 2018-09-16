; example2.nsi
;
; This script is based on example1.nsi, but it remember the directory, 
; has uninstall support and (optionally) installs start menu shortcuts.
;
; It will install makensisw.exe into a directory that the user selects,

;--------------------------------

!include "MUI2.nsh"
!include UpgradeDLL.nsh

 Function .onInit
   ReadRegStr $INSTDIR HKLM "Software\Vember Audio SURGE" "Install_Dir"
   StrCmp $INSTDIR "" 0 NoAbort
	 ReadRegStr $INSTDIR HKLM "Software\VST" "VSTPluginsPath"
   NoAbort:
 FunctionEnd

;--------------------------------
;Interface Configuration

;!define MUI_ICON ${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico
;!define MUI_UNICON ${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico
!define MUI_ICON surge.ico
!define MUI_UNICON surge.ico

!define MUI_HEADERIMAGE
;!define MUI_HEADERIMAGE_BITMAP ${NSISDIR}\Contrib\Graphics\Header\orange.bmp
;!define MUI_HEADERIMAGE_UNBITMAP ${NSISDIR}\Contrib\Graphics\Header\orange.bmp

!define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Destination VST-plugin Folder"
!define MUI_DIRECTORYPAGE_TEXT_TOP "Setup will install SURGE in the following folder. To install in a different folder, click Browse and select another folder.$\n$\nAs SURGE is a VST-plugin, you will want to install it to the folder in which your host look for plugins. (often named vstplugins)$\n$\nClick Install to start the installation."

;--------------------------------

; The name of the installer
!ifdef DEMO
Name "Surge Demo ${Version}"
!else
Name "Surge ${Version}"
!endif

; The file to write
!ifdef DEMO
OutFile "..\target\Vember Audio Surge Demo ${Version}.exe"
!else
OutFile "..\target\Vember Audio Surge ${Version}.exe"
!endif

!ifdef DEMO
!define PRODUCT_NAME "Vember Audio Surge Demo"
!define DATADIR "$LOCALAPPDATA\Vember Audio\Surge Demo"
!else
!define PRODUCT_NAME "Vember Audio Surge"
!define DATADIR "$LOCALAPPDATA\Vember Audio\Surge"
!endif

!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define SURGE_KEY "SOFTWARE\${PRODUCT_NAME}"
!define SMPATH "$SMPROGRAMS\${PRODUCT_NAME}"
!define VST3DIR "$COMMONFILES64\VST3\Vember Audio"


;SetCompressor lzma
SetCompressor bzip2

CRCCheck on
XPStyle on

;--------------------------------

; Pages

;Page license
;Page components
;Page directory
;Page instfiles

!define MUI_COMPONENTSPAGE_NODESC
!insertmacro MUI_PAGE_LICENSE "../LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES 
  
;--------------------------------
;Languages
 
!insertmacro MUI_LANGUAGE "English"

;--------------------------------

; The stuff to install

Section /o "VST2"
  
  SetOutPath $INSTDIR
  File "${SurgeDll}"

SectionEnd

Section /o "VST3"
  
  SetOutPath "${VST3DIR}"
  File "${SurgeVst3}"

SectionEnd

Section "Data"

  SectionIn RO

  ; Set output path to the installation directory.
  SetOutPath "${DATADIR}"
  
  ; Put file there
  File "..\..\..\documentation\surge.pdf"

  File "surge\surgedata\configuration.xml"
  File "surge\surgedata\surge_license.txt"
  File "surge\surgedata\windows.wt"

  CreateDirectory "${DATADIR}\patches_user"

  SetOutPath "${DATADIR}\wavetables"  
  File /r "surge\surgedata\wavetables\*.*" 
  
  !ifdef DEMO
  SetOutPath "${DATADIR}\patches_factory"
  File /r /x *.svn "surge\surgedata\demopatches_factory\*.*"
  SetOutPath "${DATADIR}\patches_3rdparty"
  File /r /x *.svn "surge\surgedata\demopatches_3rdparty\*.*"
  !else
  SetOutPath "${DATADIR}\patches_factory"
  File /r "surge\surgedata\patches_factory\*.*"
  SetOutPath "${DATADIR}\patches_3rdparty"
  File /r "surge\surgedata\patches_3rdparty\*.*"
  !endif

  ; Write the installation path into the registry
  WriteRegStr HKLM "${SURGE_KEY}" "Install_Dir" "${DATADIR}"

  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" '"${DATADIR}\surge-uninstall.exe"'
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1
  WriteUninstaller "surge-uninstall.exe"
   
SectionEnd

Section "Start Menu Shortcuts"
  CreateDirectory "${SMPATH}"  
  CreateShortCut "${SMPATH}\SURGE manual.lnk" "${DATADIR}\surge.pdf" "" "${DATADIR}\surge.pdf" 0
  CreateShortCut "${SMPATH}\Uninstall.lnk" "${DATADIR}\surge-uninstall.exe" "" "${DATADIR}\surge-uninstall.exe" 0  
  CreateShortCut "${SMPATH}\Management Patches.lnk" "%windir%\explorer.exe" "/e,/root,${DATADIR}" "%windir%\explorer.exe" 1
SectionEnd

;--------------------------------


;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "${UNINSTALL_KEY}"
  DeleteRegKey HKLM "${SURGE_KEY}"

  ; Remove files and uninstaller
  Delete "$INSTDIR\Surge.dll"
  Delete "$INSTDIR\Surge-Demo.dll"
  Delete "${VST3DIR}\Surge.vst3"
  Delete "${VST3DIR}\Surge-Demo.vst3"
  Delete "${DATADIR}\surge.pdf"
  Delete "${DATADIR}\surge-uninstall.exe"

  ; Remove shortcuts, if any
  Delete "${SMPATH}\*.*"

  ; Remove directories used
  RMDir "${SMPATH}"
  RMDir /r "${DATADIR}\wavetables"
  RMDir /r "${DATADIR}\patches_factory"
  RMDir /r "${DATADIR}\patches_3rdparty"
  RMDir "${DATADIR}\patches_user"
  Delete "${DATADIR}\*.*"
  RMDir "${DATADIR}\surgedata"
  RMDir "${DATADIR}"
SectionEnd