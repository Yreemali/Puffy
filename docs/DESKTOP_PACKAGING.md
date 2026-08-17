# Puffy desktop packaging

Puffy uses one platform-aware entry point for development and release builds:

```bash
cd ui/web
npm ci
npm run tauri:dev
npm run tauri:build
```

`scripts/tauri.mjs` configures CMake, builds `puffy_native`, stages its runtime
library, writes an ignored platform-specific Tauri config and then invokes the
local Tauri CLI. Machine-specific absolute library paths are never committed.

## Windows

Use an MSVC shell with CMake, Rust, Node.js and vcpkg available. The wrapper
selects `x64-windows-static` when `VCPKG_ROOT` or `VCPKG_INSTALLATION_ROOT` is
set.

```powershell
vcpkg install sqlite3:x64-windows-static libsndfile:x64-windows-static
cd ui/web
npm ci
npm run tauri:build -- --bundles nsis
```

The import library is used at link time and `puffy_native.dll` is bundled beside
`Puffy.exe`, which is part of the standard Windows DLL search order. CI artifacts
are unsigned; public installers require a code-signing identity.

For a signed build, import the certificate into the Windows certificate store
and set both `PUFFY_WINDOWS_CERTIFICATE_THUMBPRINT` and
`PUFFY_WINDOWS_TIMESTAMP_URL` before invoking the same build command. See
`docs/guides/SIGN_WINDOWS.md`.

Local/CI desktop builds keep the unsigned virtual microphone driver separate.
For a production release, pass a Microsoft-signed driver package to
`scripts\build-windows.ps1 -SignedDriverDirectory ...`; the wrapper verifies
the SYS/INF against the exact signed catalog with SignTool, bundles the payload,
and switches NSIS to a per-machine install so its driver hook can create/update
the virtual device. Driver installation/removal is the operation that requires
elevation.

## macOS

Use Xcode Command Line Tools, CMake, Rust, Node.js and Homebrew dependencies:

```bash
brew install sqlite3 libsndfile
export CMAKE_PREFIX_PATH="$(brew --prefix sqlite3):$(brew --prefix libsndfile)"
cd ui/web
npm ci
npm run tauri:build -- --bundles app,dmg
```

The wrapper discovers non-system dylib dependencies with `otool`, copies them to
the generated native stage and rewrites references with `install_name_tool`.
Tauri embeds those files in `Puffy.app/Contents/Frameworks`. The application has
microphone and input-monitoring usage descriptions, but users must still grant
the corresponding permissions.

CI builds an unsigned `.app`. Public distribution requires a Developer ID
Application certificate and notarization. The virtual microphone Audio Server
Plug-in/DriverKit component is signed and installed separately.

## User data

The SQLite library is stored without administrator privileges:

- Windows: `%LOCALAPPDATA%\Puffy\puffy.sqlite`;
- macOS: `~/Library/Application Support/Puffy/puffy.sqlite`;
- Linux: `$XDG_DATA_HOME/Puffy/puffy.sqlite` or `~/.local/share/Puffy/puffy.sqlite`.

On first launch, an existing `puffy.sqlite` from the working directory is copied
to the new location when the destination does not already exist.
