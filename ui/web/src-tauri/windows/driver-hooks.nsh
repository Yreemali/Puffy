; Puffy Virtual Audio release hooks.
; This file is activated only when PUFFY_SIGNED_DRIVER_DIR is supplied during
; the Tauri build. The generated NSIS installer is then per-machine/elevated.

!macro NSIS_HOOK_POSTINSTALL
  ${If} ${FileExists} "$INSTDIR\driver\PuffyVirtualAudioDevice.exe"
    DetailPrint "Installing Puffy Virtual Microphone driver..."
    ExecWait '"$INSTDIR\driver\PuffyVirtualAudioDevice.exe" install "$INSTDIR\driver\PuffyVirtualAudio.inf"' $0
    ${If} $0 == 10
      SetRebootFlag true
    ${ElseIf} $0 != 0
      MessageBox MB_ICONSTOP|MB_OK "Puffy was installed, but the signed virtual microphone driver failed to install (exit code $0). The installer cannot provide a working virtual microphone without this driver."
      Abort
    ${EndIf}
  ${Else}
    MessageBox MB_ICONSTOP|MB_OK "The release installer is missing the Puffy Virtual Microphone driver payload."
    Abort
  ${EndIf}
!macroend

!macro NSIS_HOOK_PREUNINSTALL
  ${If} ${FileExists} "$INSTDIR\driver\PuffyVirtualAudioDevice.exe"
    DetailPrint "Removing Puffy Virtual Microphone device..."
    ExecWait '"$INSTDIR\driver\PuffyVirtualAudioDevice.exe" remove' $0
    ${If} $0 == 10
      SetRebootFlag true
    ${ElseIf} $0 != 0
      MessageBox MB_ICONEXCLAMATION|MB_OK "Puffy could not remove the virtual microphone device automatically (exit code $0). The application will continue uninstalling; the signed driver package may remain in Windows Driver Store."
    ${EndIf}
  ${EndIf}
!macroend
