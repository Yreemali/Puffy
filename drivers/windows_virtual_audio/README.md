# Puffy Virtual Microphone — Windows driver

This directory contains the Windows kernel-mode virtual audio cable used by
Puffy. The implementation uses **ACX 1.1 + KMDF 1.31** and targets x64 Windows
10 version 2004 (build 19041) or newer, including Windows 11.

## Endpoint layout

```text
Puffy desktop application
        |
        | WASAPI render, float32 / 48 kHz / stereo
        v
Puffy Virtual Microphone Transport   [render endpoint]
        |
        | kernel nonpaged ring buffer (~200 ms)
        v
Puffy Virtual Microphone             [capture endpoint]
        |
        +--> Discord / OBS / games / browser / other WASAPI clients
```

The endpoint names and audio transport format must remain synchronized with
`virtual_audio/virtual_device_contract.hpp`.

The cable has deliberate low-latency failure behavior:

- an empty cable produces digital silence instead of stale audio;
- an overflowing cable discards the oldest complete audio frames;
- only complete float32 stereo frames enter or leave the ring;
- render and capture packet processing is synchronized with a kernel spin lock;
- a render WaveRT slot is read only after AudioKSE releases that exact logical
  packet; a missing/late packet advances as silence instead of replaying stale
  memory from a reused slot;
- WaveRT packet buffers are consumed/filled in 2 ms sub-packet chunks, and the
  presentation position/QPC pair only advances after those bytes have actually
  moved through the cable;
- a scheduler stall shifts the virtual clock instead of causing an unbounded
  burst of stale packet completions.

## Source files

- `PuffyVirtualAudio.cpp/.hpp` — ACX/KMDF device, circuits, WaveRT packet
  handling and in-kernel cable.
- `PuffyVirtualAudio.inf` — root-enumerated MEDIA device and the two audio
  interfaces.
- `PuffyVirtualAudio.vcxproj/.sln` — Visual Studio/WDK x64 driver project.
- `PuffyVirtualAudioDevice.cpp/.vcxproj` — small SetupAPI/NewDev helper that
  creates the root-enumerated device on first install and binds the INF to it.
- `build-driver.ps1` — builds the SYS/INF/CAT package plus the device helper.
- `install-driver.ps1` — validates the signed catalog and invokes the device
  helper from an elevated PowerShell session.
- `uninstall-driver.ps1` — removes the root-enumerated Puffy device.
- `ui/web/src-tauri/windows/driver-hooks.nsh` — installs/removes a production-
  signed driver from the elevated NSIS release installer.

## Build prerequisites

Install on an x64 Windows development machine:

1. Visual Studio 2022 with **Desktop development with C++**.
2. A current Windows 10/11 SDK and Windows Driver Kit (WDK) that includes
   ACX 1.1.
3. KMDF 1.31 support (provided by the matching modern WDK).

Then run from PowerShell:

```powershell
cd drivers\windows_virtual_audio
.\build-driver.ps1 -Configuration Release -Clean
```

Expected package output:

```text
build\x64\Release\PuffyVirtualAudio.sys
build\x64\Release\PuffyVirtualAudio.inf
build\x64\Release\PuffyVirtualAudio.cat
build\x64\Release\PuffyVirtualAudioDevice.exe
```

The build helper always regenerates the catalog after the final SYS/INF staging
step and validates the package with Inf2Cat for supported x64 Windows 10/11
release identifiers. Creating a CAT file does **not** make the driver
production-signed.

## Install

Normal Windows systems require a trusted, correctly signed kernel driver
package. Once the CAT/package has a valid trusted signature, open PowerShell as
Administrator and run:

```powershell
.\install-driver.ps1 -Configuration Release
```

The script intentionally refuses an untrusted catalog. On first installation,
`PuffyVirtualAudioDevice.exe` creates the `ROOT\PuffyVirtualAudio` devnode with
SetupAPI and then binds the INF using NewDev. This is necessary because adding a
driver package alone does not create a root-enumerated device instance. The
installer does not modify boot configuration, disable Secure Boot, or enable
test-signing mode.

After a successful install, Windows should expose:

- playback/render: `Puffy Virtual Microphone Transport`;
- recording/capture: `Puffy Virtual Microphone`.

Puffy writes mixed audio to the first endpoint. Other applications select the
second endpoint as their microphone.

## Development validation

For driver development, use Microsoft's normal WDK deployment/debugging flow on
an isolated test machine or VM. Before release, test at minimum:

- repeated start/stop/pause of both endpoints;
- Discord/OBS recording while Puffy sends silence and active audio;
- long playback runs for glitches/drift;
- rapid endpoint open/close and application crashes;
- sleep/resume and device disable/enable;
- install, upgrade and uninstall on supported Windows 10 and Windows 11 builds;
- Driver Verifier on a dedicated test system.

The repository's Linux/macOS builds can validate source consistency, but they
cannot compile, load or runtime-test a Windows kernel driver.


## Production NSIS integration

The normal local build does **not** bundle an unsigned kernel driver. After the
SYS/INF/CAT package has completed the Microsoft production signing path, build
the release installer with:

```powershell
.\scripts\build-windows.ps1 -Clean -SkipDriver `
  -SignedDriverDirectory C:\path\to\signed\PuffyVirtualAudio
```

The build verifies that the catalog has a valid trusted signature and uses
SignTool kernel-policy catalog verification for the supplied SYS/INF, bundles
the four driver payload files, switches NSIS to `perMachine` (elevated) mode,
and activates install/uninstall hooks. The post-install hook creates or updates
`ROOT\PuffyVirtualAudio`; the pre-uninstall hook removes the device.
