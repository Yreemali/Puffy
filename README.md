![Build](https://img.shields.io/badge/build-experimental-yellow)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![UI](https://img.shields.io/badge/UI-React%20%2B%20Tauri-purple)
![Audio](https://img.shields.io/badge/audio-realtime-orange)
![Linux](https://img.shields.io/badge/Linux-PipeWire-green)
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

> ⚠️ **Warning:** Linux is the currently implemented platform. Windows and macOS
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
npm install
npm run build
```

Run the Tauri desktop window during development:

```bash
cd ui/web
npm run tauri:dev
```

The web renderer is only the interface. Native playback, device access, hotkeys
and real-time audio cross the bridge into the C++/Rust host layer.

## Build the C++ core

```bash
cmake -S . -B build \
  -DPUFFY_BUILD_TESTS=ON \
  -DPUFFY_BUILD_DESKTOP=OFF

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For the Qt desktop target:

```bash
cmake -S . -B build-desktop \
  -DPUFFY_BUILD_DESKTOP=ON \
  -DPUFFY_BUILD_TESTS=ON

cmake --build build-desktop --parallel
ctest --test-dir build-desktop --output-on-failure
```

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

WASAPI/MMDevice and Core Audio can provide physical capture and monitoring, but a
system-wide microphone endpoint requires an additional platform component:

- Windows: a properly signed virtual audio driver;
- macOS: an Audio Server Plug-in or DriverKit component with signing and entitlements.

Those components are separate engineering projects. The repository contains
interfaces and architecture notes, not imaginary drivers assembled from optimism.

## Repository layout

```text
core/                 portable audio, mixer, effects and playback logic
native/               native bridge used by the web desktop app
platform/linux/      PipeWire, X11 and evdev integrations
platform/windows/    Windows backend contracts
platform/macos/      macOS backend contracts
ui/web/               React/Tauri desktop interface
ui/qml/               legacy Qt interface
apps/desktop/         native desktop entry point
tests/                core tests and mocks
docs/                 architecture and platform notes
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
