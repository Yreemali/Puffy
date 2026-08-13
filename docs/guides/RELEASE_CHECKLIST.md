# Puffy release checklist

## Common

- [ ] Version matches in CMake, Cargo, Tauri, npm and platform packages.
- [ ] Changelog contains user-visible changes and known limitations.
- [ ] C++ tests, frontend build and Tauri package jobs pass.
- [ ] Fresh profile, migration and missing-device recovery are tested.
- [ ] No certificate, key, password, token, local database or audio file is
      present in the commit or artifact.
- [ ] SHA-256 checksums are generated after final signing/notarization.
- [ ] GPL license, corresponding source and dependency notices are included.

## Audio validation

- [ ] Physical microphone can be disconnected and reconnected safely.
- [ ] Monitoring and virtual output have independent volume and mute controls.
- [ ] Multiple sounds stop at EOF and Stop All clears every active voice.
- [ ] MP3, WAV, FLAC and OGG imports are tested.
- [ ] Hotkeys never suppress ordinary keyboard input.
- [ ] Full Keyboard Mode and repeat/blacklist settings survive restart.

## Linux

- [ ] Arch package builds in a clean environment.
- [ ] AppImage runs on the declared oldest supported distribution.
- [ ] PipeWire virtual source is visible in a second application.
- [ ] AppImage/package signatures and public fingerprints are published.

## Windows

- [ ] Native build and tests pass on Windows 10 and Windows 11.
- [ ] EXE, DLL and final installer have valid timestamped Authenticode signatures.
- [ ] Virtual-audio driver package has the required Microsoft signature.
- [ ] Driver install/uninstall is tested with Secure Boot and HVCI enabled.
- [ ] Main application runs without elevation.

## macOS

- [ ] Intel and Apple Silicon targets are covered or the release declares one.
- [ ] Nested code and `Puffy.app` pass `codesign --verify --deep --strict`.
- [ ] Gatekeeper assessment passes and notarization is stapled.
- [ ] Microphone and Input Monitoring permission flows are tested.
- [ ] Virtual-audio component installs on a clean Mac without disabling SIP.

> If a release needs users to turn off core operating-system security to make a
> soundboard work, that release is not ready. The airhorn can wait.
