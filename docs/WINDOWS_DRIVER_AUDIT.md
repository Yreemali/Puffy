# Windows virtual microphone audit

Audit date: 2026-08-18

## Scope

This review checks the Puffy ACX virtual microphone source, its WaveRT packet protocol,
root-device installer helper, INF/catalog generation, signed-driver release packaging,
and the desktop application's WASAPI transport integration.

## Fixed during the audit

- Fixed a duplicated `else` in `configureStream()` that prevented the driver from compiling.
- Removed duplicate `UNICODE` / `_UNICODE` preprocessor definitions from the device helper;
  MSBuild already supplies them and the project builds with warnings-as-errors.
- Reworked timer-driven WaveRT movement into 2 ms sub-packet transfers. Presentation position
  now advances together with actual render/capture buffer consumption instead of only once per packet.
- Added exact logical render-packet readiness tracking so the driver never reads an AudioKSE-owned
  or stale reused WaveRT slot. Missing/late render data is converted to digital silence.
- Added EOS-aware valid-byte handling without imposing an undocumented frame-alignment requirement
  on `EosPacketLength`; incomplete trailing frames are never copied into the frame-based cable.
- Hardened RT packet allocation with checked integer arithmetic and page-aligned allocation.
- Regenerate the catalog after every driver build so an incremental build cannot accidentally reuse
  a catalog for old SYS/INF bytes.
- Release packaging now verifies SYS and INF against the exact signed CAT with SignTool kernel policy
  before a driver package is embedded into NSIS.
- Signed driver resources and elevated NSIS install/uninstall hooks are wired into the release build.
- Bumped the driver package version to 0.3.2.0.
- Expanded `scripts/check-windows-driver-source.py` so the regressions above are checked automatically.

## Verified in this environment

- Cross-platform Windows-driver source checker passes.
- Tauri build script passes Node syntax validation.
- Tauri JSON and both Visual Studio project XML files parse successfully.
- Portable CMake build succeeds.
- `puffy_core_tests`: 1/1 passed.
- Application code resolves `Puffy Virtual Microphone Transport`, opens it as a WASAPI render endpoint,
  and passes the virtual microphone into `AudioEngine`, which writes the final virtual mix to it.

## Still requires a Windows WDK validation pass

This Linux environment cannot compile or load a KMDF/ACX `.sys`. Before a production release, run the
WDK build on a Windows 10/11 test machine, run Inf2Cat/SignTool, install the package, verify both endpoints
in Windows audio settings, and exercise render -> capture audio with Driver Verifier enabled during testing.
A production package must use the appropriate Microsoft driver-signing process.
