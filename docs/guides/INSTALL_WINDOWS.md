# Installing Puffy on Windows 10/11

Puffy uses WebView2 for its interface and WASAPI/MMDevice for audio. The normal
application runs without administrator privileges.

## Install a release build

1. Download the signed `Puffy_*_x64-setup.exe` from the project release.
2. Verify its Authenticode signature in file Properties → Digital Signatures.
3. Run the NSIS installer and launch Puffy from the Start menu.
4. Allow microphone access when Windows asks.

Current CI artifacts are unsigned development builds. Do not treat a
SmartScreen override as signature verification.

## Build locally

Install:

- Visual Studio 2022 Build Tools with **Desktop development with C++**;
- Windows 10/11 SDK;
- CMake, Rust stable, Node.js 22 and vcpkg;
- WebView2 Runtime when it is not already present.

From an MSVC developer PowerShell:

```powershell
vcpkg install sqlite3:x64-windows-static libsndfile:x64-windows-static
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
Set-Location ui\web
npm ci
npm run tauri:build -- --bundles nsis
```

The wrapper builds `puffy_native.dll`, stages it beside `Puffy.exe`, and creates
the installer under `src-tauri\target\release\bundle\nsis`.

## Virtual microphone

The desktop installer alone cannot create a Windows capture endpoint. A release
must include the separately built and Microsoft-signed virtual-audio driver.
Only its install/update/remove helper may request elevation.

For a production driver package, from an elevated terminal and only after
verifying its signature:

```powershell
pnputil /add-driver .\PuffyVirtualAudio.inf /install
```

Test-signed drivers belong on disposable test machines, not user systems.

## Application data

Puffy stores its database under `%LOCALAPPDATA%\Puffy`. Uninstalling the app does
not silently delete the user's library metadata.

## References

- [Tauri prerequisites](https://v2.tauri.app/start/prerequisites/)
- [Tauri Windows installers](https://v2.tauri.app/distribute/windows-installer/)
- [Microsoft PnPUtil driver installation](https://learn.microsoft.com/windows-hardware/drivers/install/test-signing)
