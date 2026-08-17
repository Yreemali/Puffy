# Changelog

## Unreleased

* Added the C++20 core foundation, mixer routing, hotkey dispatch, and Full Keyboard Mode controller.
* Added architecture and virtual-device feasibility documentation.
- Added MP3 import and decoding through the installed libsndfile backend.
- Added event-driven WASAPI capture/output and MMDevice enumeration on Windows.
- Added AUHAL capture/output and Core Audio device enumeration on macOS.
- Added passive Windows low-level-hook and macOS Quartz event-tap keyboard listeners.
- Added the shared lock-free audio transport and virtual-device component contracts.
- Retired the legacy Qt/QML frontend; React/Tauri is now the only desktop UI.
- Added lazy native waveform extraction from decoded audio and realtime mixer bus meters.
- Rebuilt All Sounds filtering, Soundboard importing, Mixer channel strips, theme tokens, and structured settings.
- Added platform-aware Tauri native staging, Windows DLL exports, macOS `@rpath` dependency bundling, and unsigned Windows/macOS CI artifacts.
- Moved SQLite into each platform's per-user application-data directory with migration from the legacy working-directory database.
