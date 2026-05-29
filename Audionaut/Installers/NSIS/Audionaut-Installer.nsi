!define Name "Audionaut"
Name "${Name}"
Outfile "${Name} Setup.exe"
RequestExecutionLevel admin ;Require admin rights on NT6+ (When UAC is turned on)
InstallDir "$ProgramFiles64\${Name}"
;"..\..\Builds\VisualStudio2026\icon.ico"

!include LogicLib.nsh
!include MUI.nsh
!include "MUI2.nsh"

Function .onInit
SetShellVarContext all
UserInfo::GetAccountType
pop $0
${If} $0 != "admin" ;Require admin rights on NT4+
    MessageBox mb_iconstop "Administrator rights required!"
    SetErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
    Quit
${EndIf}
FunctionEnd

Function LaunchLink
  ExecShell "" "$SMPROGRAMS\${Name}.lnk"
FunctionEnd

!define MUI_ICON "..\..\Builds\VisualStudio2026\icon.ico"
!define MUI_HEADERIMAGE
; TODO !define MUI_HEADERIMAGE_BITMAP "path\to\InstallerLogo.bmp"
!define MUI_HEADERIMAGE_RIGHT


!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Launch Audionaut.exe"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchLink"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"


Section
SetOutPath "$INSTDIR"
WriteUninstaller "$INSTDIR\Uninstall.exe"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Voltmer Systems"   "DisplayName" "${Name}"
WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Voltmer Systems"   "UninstallString" "$INSTDIR\Uninstall.exe"
;Install your files with the File command
File "..\..\Builds\VisualStudio2026\x64\Release\App\${Name}.exe"
CreateShortCut "$SMPROGRAMS\${Name}.lnk" "$INSTDIR\${Name}.exe"
SectionEnd

Section "Uninstall"
;Delete your files
Delete "$INSTDIR\${Name}.exe"
Delete "$SMPROGRAMS\${Name}.lnk"
DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Voltmer Systems"
Delete "$INSTDIR\Uninstall.exe"
RMDir "$INSTDIR"
SectionEnd

