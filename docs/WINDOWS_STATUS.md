# Windows readiness status

This document separates the implemented Windows path from the remaining release
work.

## Implemented in this repository

### Desktop application

- Windows 10/11 desktop host through Tauri 2 and WebView2.
- x64 C++20 native library built with CMake and vcpkg.
- WASAPI/MMDevice physical microphone capture and monitoring output.
- Global keyboard handling through a passive `WH_KEYBOARD_LL` hook.
- SQLite application data under `%LOCALAPPDATA%\Puffy`.
- NSIS installer generation.
- `puffy_native.dll` staged beside `Puffy.exe`.
- Windows build helper at `scripts\build-windows.ps1`.

### Puffy Virtual Microphone driver

`drivers\windows_virtual_audio` now contains an ACX 1.1 / KMDF 1.31 x64 driver
source project instead of only a transport contract.

It implements two endpoints:

1. `Puffy Virtual Microphone Transport` — a render endpoint opened by Puffy.
2. `Puffy Virtual Microphone` — a capture endpoint selected by Discord, OBS,
   games and other clients.

The driver accepts the same fixed transport format used by the application:
float32, 48 kHz, stereo. WaveRT buffers are transferred in 2 ms sub-packet
chunks through a nonpaged kernel ring buffer. Presentation position and QPC are
published as a coherent pair only after bytes have actually been consumed or
produced. Underrun produces silence; overflow discards the oldest complete
frames to prevent unbounded latency growth.

The driver folder also contains a small statically linked SetupAPI/NewDev device
helper. It creates `ROOT\PuffyVirtualAudio` on first install before binding the
signed INF, so a clean machine does not depend on DevCon or a pre-existing
virtual device.

The full Windows build helper builds the virtual-audio package by default. Use
`-SkipDriver` only when an application-only build is intentional.

## Still required before a public release

### 1. Compile and runtime qualification on real Windows

The source must be compiled with Visual Studio + WDK and exercised on actual
Windows 10/11 systems. Cross-platform CI cannot prove that a kernel audio driver
loads, enumerates correctly, remains glitch-free, or passes Driver Verifier.

Required smoke tests include:

- both Puffy endpoints appear with the expected friendly names;
- Puffy can open the transport render endpoint;
- Discord/OBS can record from `Puffy Virtual Microphone`;
- silence, repeated playback and long playback runs behave correctly;
- sleep/resume, device disable/enable and app crashes do not wedge the endpoint;
- install/update/uninstall work on supported Windows versions.

### 2. Microsoft production driver signing

`build-driver.ps1` produces SYS/INF/CAT package material, but a generated catalog
is not a production signature. The kernel driver package must go through the
appropriate Microsoft signing path before normal end-user distribution.

`install-driver.ps1` intentionally requires a valid trusted Authenticode
signature and does not weaken Windows boot/signing security.

### 3. Production-signed driver payload for NSIS

The NSIS integration is implemented, but it is intentionally activated only when
a trusted signed driver package is supplied with `-SignedDriverDirectory`. In
that mode the installer runs per-machine/elevated, installs the virtual device in
a post-install hook, and removes the root device during uninstall. An unsigned
local WDK build is never silently bundled.

The remaining release task is obtaining the real Microsoft-signed driver payload
and exercising the resulting installer on supported Windows versions.

### 4. Public code signing for the desktop application

The desktop executable/DLL/NSIS installer also need the normal application code
signature for a polished public release. This is separate from kernel-driver
signing.

## Build command

For a complete local Windows build with the WDK installed:

```powershell
.\scripts\build-windows.ps1 -Clean
```

For an application-only build:

```powershell
.\scripts\build-windows.ps1 -Clean -SkipDriver
```

The complete build reports the application, native DLL, NSIS installer and the
virtual-audio SYS/INF/CAT output paths.


## Release installer with signed virtual microphone

```powershell
.\scripts\build-windows.ps1 -Clean -SkipDriver `
  -SignedDriverDirectory C:\path\to\signed\PuffyVirtualAudio
```

`-SignedDriverDirectory` must contain `PuffyVirtualAudio.sys`, `.inf`, `.cat`,
and `PuffyVirtualAudioDevice.exe`. The build refuses a catalog that Windows does not report as trusted and also
uses SignTool `/kp /c` checks to prove that the supplied SYS and INF are members
of that exact kernel-policy-valid catalog.
