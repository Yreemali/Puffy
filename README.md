![Build](https://img.shields.io/badge/build-experimental-yellow)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![UI](https://img.shields.io/badge/UI-React%20%2B%20Tauri-purple)
![Audio](https://img.shields.io/badge/audio-realtime-orange)
![Linux](https://img.shields.io/badge/Linux-PipeWire-green)
![Windows](https://img.shields.io/badge/Windows-WASAPI-0078d4)
![macOS](https://img.shields.io/badge/macOS-CoreAudio-black)
![License](https://img.shields.io/badge/license-GPL--3.0--or--later-blue)
![Stability](https://img.shields.io/badge/stability-your%20mileage%20may%20vary-red)

# Puffy

An experimental Soundpad-like soundboard that mixes your microphone with sounds,
sends the result to a virtual microphone, and wraps the whole thing in a cute UI
so nobody notices the audio graph screaming underneath.

> ⚠️ **Warning:** Puffy touches real-time audio, global keyboard events, SQLite,
> PipeWire and platform-specific device APIs. It may produce clicks, dropouts,
> feedback, confused audio routes or a Discord call that suddenly becomes much
> more interesting.

> ⚠️ **Warning:** Full Keyboard Mode reacts to key events. It does not store typed
> text, keyboard history or telemetry. Your keyboard is used as a sound trigger,
> not as a diary.

> ⚠️ **Warning:** Windows and macOS application backends are implemented, but they
> need proper virtual-audio drivers/components before they can expose a real
> system-wide virtual microphone. A fake endpoint would only be a very confident
> lie.

## What is this?

Puffy is an open-source soundboard and microphone mixer for people who need to
route sounds to themselves, to a call, or to both at once:

```text
Physical microphone
        +
Soundboard sounds
        ↓
Realtime mixer
   ┌────┴────┐
   ↓         ↓
Headphones  Virtual microphone
```

Every sound can be routed to:

- headphones only;
- virtual microphone only;
- both;
- nowhere, if the sound has made poor life choices.

## Features

- C++20 real-time audio core.
- React + TypeScript + Vite web UI inside a Tauri desktop window.
- SQLite sound library, profiles and playlists.
- WAV, MP3, FLAC, OGG and formats supported by the installed decoder backend.
- Simultaneous playback with per-sound volume and routing.
- Restart, overlap, ignore, toggle and hold playback modes.
- Microphone capture, monitoring and virtual-microphone mixing.
- Gain, noise gate, compressor, limiter, low-pass, high-pass and delay effects.
- Full Keyboard Mode with random, sequential and single-sound behavior.
- Modifier blacklist and configurable OS key-repeat behavior.
- Playlist playback modes, auto-continue and repeat-current behavior.
- Soundboard layouts: Compact, Normal and Large.
- Local profiles with avatar, theme and customizable palette.
- Import/export of local configuration.
- Linux PipeWire audio backend.
- X11 global keyboard listener, with Wayland limitations documented honestly.
- Windows WASAPI/MMDevice backend and passive low-level keyboard hook.
- macOS Core Audio/AUHAL backend and passive Quartz keyboard event tap.

## Platform reality check

| Platform | Desktop app | Physical audio | Global keys | Virtual microphone |
| --- | --- | --- | --- | --- |
| Linux | Working development target | PipeWire | X11 + evdev fallback | PipeWire virtual source |
| Windows 10/11 | Native backend and package pipeline | WASAPI/MMDevice | Passive low-level hook | Requires the separate signed driver |
| macOS 12+ | Native backend and package pipeline | Core Audio/AUHAL | Passive event tap + permission | Requires the separate signed audio component |

The application itself never needs administrator privileges. Elevation belongs
only to installing, updating or removing the platform virtual-audio component.

## The audio pipeline

```text
Microphone capture ──→ microphone effects ──┐
                                           ├─→ mixer ─→ virtual microphone
Sound playback ─────→ per-sound routing ───┘       └─→ local monitoring
```

The audio callback is expected to remain boring. It must not perform:

- allocations;
- filesystem or database I/O;
- blocking mutex operations;
- synchronous logging;
- UI calls.

If the callback starts allocating memory, the gremlins win.

## Build the web desktop app

```bash
cd ui/web
npm ci
npm run build
```

Run the Tauri desktop window during development:

```bash
cd ui/web
npm run tauri:dev
```

The Tauri commands build and stage the platform C++ library automatically. On
Windows, `puffy_native.dll` is installed beside the executable. On macOS, the
native dylib and its non-system dependencies are embedded in
`Puffy.app/Contents/Frameworks` with relocatable `@rpath` references.

Unsigned platform packages can be produced with:

```text
Windows: npm run tauri:build -- --bundles nsis
macOS:   npm run tauri:build -- --bundles app,dmg
Linux:   npm run tauri:build -- --bundles appimage
```

Platform-specific installation and release instructions live here:

- [Linux installation](docs/guides/INSTALL_LINUX.md)
- [Windows installation](docs/guides/INSTALL_WINDOWS.md)
- [macOS installation](docs/guides/INSTALL_MACOS.md)
- [Linux artifact signing](docs/guides/SIGN_LINUX.md)
- [Windows application and driver signing](docs/guides/SIGN_WINDOWS.md)
- [macOS signing and notarization](docs/guides/SIGN_MACOS.md)
- [Release checklist](docs/guides/RELEASE_CHECKLIST.md)

The web renderer is only the interface. Native playback, device access, hotkeys
and real-time audio cross the bridge into the C++/Rust host layer.

## Build the C++ core

```bash
cmake -S . -B build \
  -DPUFFY_BUILD_TESTS=ON

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The CMake project builds the portable C++ engine and native bridge. The only
desktop interface is the React/Tauri application under `ui/web`; the former
Qt/QML frontend has been retired.

## Linux status

Linux is the main development target. Puffy uses PipeWire for capture,
monitoring and the virtual microphone source:

```text
OpenSoundboard Virtual Microphone
```

PipeWire must be available in the user session. X11 global hotkeys are supported
where the XRecord extension is available. Wayland global keyboard capture depends
on the compositor and its security model; Puffy does not bypass that model.

## Windows and macOS status

WASAPI/MMDevice and Core Audio backends provide physical capture, monitoring,
device enumeration and global-key handling. A
system-wide microphone endpoint requires an additional platform component:

- Windows: a properly signed virtual audio driver;
- macOS: an Audio Server Plug-in or DriverKit component with signing and entitlements.

The app-side transport senders and component contracts are implemented under
`platform/` and `drivers/`. Production driver binaries still require native SDK
builds, signing, installation tests and — regrettably — paperwork.

GitHub Actions builds unsigned Windows and macOS artifacts so platform compiler
failures are visible early. Unsigned CI artifacts are development output, not a
promise that Gatekeeper, SmartScreen or a kernel will suddenly become trusting.

## Repository layout

```text
core/                 portable audio, mixer, effects and playback logic
native/               native bridge used by the web desktop app
platform/linux/      PipeWire, X11 and evdev integrations
platform/windows/    WASAPI and Windows keyboard backend
platform/macos/      Core Audio and macOS keyboard backend
drivers/             virtual-device component contracts
virtual_audio/       shared virtual-device transport identity
ui/web/               React/Tauri desktop interface
apps/desktop/         native desktop entry point
tests/                core tests and mocks
docs/                 architecture and platform notes
docs/guides/          installation, signing and release instructions
packaging/arch/       Arch Linux packaging files
```

## Privacy

Puffy is local-first. It does not intentionally store:

- typed text;
- keyboard history;
- microphone recordings;
- remote user profiles;
- telemetry without explicit consent.

Keyboard events are transient control signals. They enter the hotkey router and
are forgotten when they are no longer needed.

## License

Puffy is free software released under the GNU General Public License, version 3
or any later version. See [`LICENSE`](LICENSE).

No warranty is provided. Especially not for your audio routing, your hotkeys, or
the emotional consequences of pressing Full Keyboard Mode in a meeting.

> **Good luck building it.**
