# Installing Puffy on Windows 10/11

Puffy uses WebView2 for its interface and WASAPI/MMDevice for application audio.
The normal desktop process runs without administrator privileges. Installing a
kernel audio driver is the one part that requires elevation.

## Install a release build

A complete release consists of two independently signed parts:

1. the Puffy desktop/NSIS application package;
2. the Puffy Virtual Audio kernel-driver package.

For a production release, verify the publisher/signature of both packages
before installation.

## Build locally

Install:

- Visual Studio 2022 with **Desktop development with C++**;
- a modern Windows 10/11 SDK;
- Windows Driver Kit (WDK) with ACX 1.1 / KMDF 1.31 for the virtual microphone;
- CMake, Rust stable, Node.js 22 and vcpkg;
- WebView2 Runtime when it is not already present.

From the repository root:

```powershell
.\scripts\build-windows.ps1 -Clean
```

This builds the virtual microphone driver, installs the x64 static vcpkg
libraries, builds/tests the C++ core, builds the Tauri/NSIS package, and checks
that all expected artifacts exist.

Use these switches only when needed:

```powershell
# Skip C++ tests
.\scripts\build-windows.ps1 -SkipTests

# Build only the desktop app when WDK/driver output is intentionally not needed
.\scripts\build-windows.ps1 -SkipDriver
```

The desktop installer is created under:

```text
ui\web\src-tauri\target\release\bundle\nsis
```

The driver development package is created under:

```text
drivers\windows_virtual_audio\build\x64\Release
```

## Build only the virtual microphone

```powershell
cd drivers\windows_virtual_audio
.\build-driver.ps1 -Configuration Release -Clean
```

Expected files:

```text
PuffyVirtualAudio.sys
PuffyVirtualAudio.inf
PuffyVirtualAudio.cat
PuffyVirtualAudioDevice.exe
```

The driver exposes:

- `Puffy Virtual Microphone Transport` as the private render sink used by Puffy;
- `Puffy Virtual Microphone` as the recording endpoint selected by other apps.

The transport format is float32 / 48 kHz / stereo, matching
`virtual_audio\virtual_device_contract.hpp`.

## Install the virtual microphone

A generated CAT file is not, by itself, a production signature. On a normally
configured end-user machine, install only a package whose catalog has a valid
trusted signature.

After signing/trusting the development or production package, open PowerShell as
Administrator:

```powershell
cd drivers\windows_virtual_audio
.\install-driver.ps1 -Configuration Release
```

The PowerShell helper validates the package and its catalog signature. It then
uses the bundled `PuffyVirtualAudioDevice.exe` SetupAPI/NewDev helper to create
`ROOT\PuffyVirtualAudio` when needed and bind the INF. It deliberately does not
enable test-signing, disable Secure Boot, or change boot policy.

Restart Puffy plus any already-running audio client after installation. In a
recording application's microphone list, select `Puffy Virtual Microphone`, not
the `Transport` endpoint.

## Application data

Puffy stores its database under `%LOCALAPPDATA%\Puffy`. Uninstalling the desktop
application does not silently delete the user's library metadata.
