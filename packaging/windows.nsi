Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!ifndef BUILD
!error "Set BUILD to the Release artefacts directory"
!endif
!ifndef OUTPUT
!error "Set OUTPUT to the installer file"
!endif
!define PRODUCT "Swell - Natural Synthetiser"
!define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\SwellNaturalSynthetiser"
Name "${PRODUCT} 2.0.2"
OutFile "${OUTPUT}"
InstallDir "$PROGRAMFILES64\Swell\Natural Synthetiser"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
VIProductVersion 2.0.2.0
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "ProductVersion" "2.0.2"
VIAddVersionKey "FileVersion" "2.0.2"
VIAddVersionKey "FileDescription" "Swell Natural Synthetiser Setup"
VIAddVersionKey "LegalCopyright" "Swell Audio"
!define MUI_WELCOMEPAGE_TEXT "Cet assistant installe l’instrument VST3 et son application autonome. Téléchargez aussi Swell-Library.zip et extrayez la banque. Au premier lancement, choisissez le dossier Swell Library."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Lire le guide du premier lancement"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "French"

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK "Windows 64 bits est requis."
        Abort
    ${EndIf}
FunctionEnd

Section "Instrument"
    SetRegView 64
    SetShellVarContext all
    SetOutPath "$INSTDIR"
    File "${BUILD}\Standalone\${PRODUCT}.exe"
    File "README.txt"
    SetOutPath "$COMMONFILES64\VST3\${PRODUCT}.vst3"
    File /r "${BUILD}\VST3\${PRODUCT}.vst3\*"
    CreateDirectory "$SMPROGRAMS\Swell Audio"
    CreateShortCut "$SMPROGRAMS\Swell Audio\${PRODUCT}.lnk" "$INSTDIR\${PRODUCT}.exe"
    CreateShortCut "$SMPROGRAMS\Swell Audio\Guide Natural Synthetiser.lnk" "$INSTDIR\README.txt"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${REGKEY}" "DisplayName" "${PRODUCT}"
    WriteRegStr HKLM "${REGKEY}" "Publisher" "Swell Audio"
    WriteRegStr HKLM "${REGKEY}" "DisplayVersion" "2.0.2"
    WriteRegStr HKLM "${REGKEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${REGKEY}" "DisplayIcon" "$INSTDIR\${PRODUCT}.exe"
    WriteRegStr HKLM "${REGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
SectionEnd

Section "Uninstall"
    SetRegView 64
    SetShellVarContext all
    Delete "$SMPROGRAMS\Swell Audio\${PRODUCT}.lnk"
    Delete "$SMPROGRAMS\Swell Audio\Guide Natural Synthetiser.lnk"
    RMDir "$SMPROGRAMS\Swell Audio"
    RMDir /r "$COMMONFILES64\VST3\${PRODUCT}.vst3"
    Delete "$INSTDIR\${PRODUCT}.exe"
    Delete "$INSTDIR\README.txt"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    DeleteRegKey HKLM "${REGKEY}"
SectionEnd
