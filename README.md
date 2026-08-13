![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Language](https://img.shields.io/badge/language-C%2B%2B20-blue)
![UI](https://img.shields.io/badge/UI-Qt%206%20%2B%20QML-purple)
![Audio](https://img.shields.io/badge/audio-real--time-orange)
![Linux](https://img.shields.io/badge/Linux-PipeWire-green)
![Status](https://img.shields.io/badge/status-experimental-yellow)

# puffy

Experimental cross-platform soundboard and microphone mixer written in C++20.

It plays sounds locally, mixes them with a physical microphone, and sends the result to a virtual microphone. The interface is intentionally soft and cute. The audio thread is not.

> ⚠️ **Warning:** puffy touches real-time audio, global keyboard events, SQLite,
> PipeWire and platform-specific device APIs. A bad callback can produce clicks,
> dropouts, feedback or a very annoyed Discord call.

> ⚠️ **Warning:** Full Keyboard Mode listens for key events only to trigger sounds.
> It does not store typed text, keyboard history or telemetry. Normal key input is
> not suppressed by default.

> ⚠️ **Warning:** Linux is the currently implemented platform. Windows and macOS
> backend contracts exist, but their system virtual microphone drivers are not
> included yet.

## What is puffy?

puffy is an open-source Soundpad-like soundboard for people who want to route:

```text
Physical microphone
        +
Soundboard voices
        ↓
Realtime mixer
   ┌────┴────┐
   ↓         ↓
Headphones  Virtual microphone
```

Each sound can be routed to:

- headphones only;
- virtual microphone only;
- both.

That means you can send an airhorn to Discord without hearing the airhorn in your own headphones. Tiny feature. Very important feature.

## Current features

- C++20 core with CMake.
- Qt 6/QML desktop interface.
- SQLite sound library, profiles and playlists.
- WAV, FLAC, OGG and other formats supported by the installed `libsndfile` build.
- Decoded audio cache with memory limit and eviction.
- Simultaneous sound voices.
- Sound routing: headphones, virtual microphone, both.
- Per-sound volume, loop, speed, fade-in and fade-out.
- Restart, overlap, ignore-if-playing, toggle and hold playback modes.
- Microphone mixing and monitoring.
- Gain, noise gate, compressor, limiter, low-pass, high-pass and delay effects.
- Master output limiter.
- Full Keyboard Mode:
  - random sounds;
  - sequential playlists;
  - single sound;
  - no-immediate-repeat;
  - configurable modifier/key blacklist;
  - configurable OS key repeat behavior.
- Linux PipeWire capture, monitoring output and virtual microphone source.
- Linux X11 global keyboard listener.
- JSON profile export/import.
- System tray commands.
- CPack and GitHub Actions foundation.

## Linux virtual microphone

On Linux, puffy creates a PipeWire source named:

```text
OpenSoundboard Virtual Microphone
```

The source carries:

```text
processed physical microphone
    +
soundboard audio
```

Select it as the input device in Discord, OBS, TeamSpeak or another PipeWire/PulseAudio-compatible application.

PipeWire must be available in the user session. If it is not available, puffy should enter a degraded state instead of pretending that a virtual device exists.

## The cursed parts

puffy deliberately separates portable audio logic from platform code:

```text
core/
  mixer, effects, soundboard, library, profiles, hotkeys

platform/linux/
  PipeWire capture/output/virtual microphone
  X11 global keyboard listener

platform/windows/
  WASAPI and virtual-device contracts

platform/macos/
  Core Audio and virtual-device contracts
```

The audio callback must not perform:

- allocations;
- database queries;
- filesystem I/O;
- blocking mutex operations;
- synchronous logging;
- UI calls.

If the callback starts allocating memory, the gremlins win.

## Build

### Core and tests

```bash
cmake -S . -B build \
  -DPUFFY_BUILD_TESTS=ON \
  -DPUFFY_BUILD_DESKTOP=OFF

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Qt desktop application

Dependencies include:

- CMake 3.21+;
- C++20 compiler;
- Qt 6.5+ with Core, Gui, Qml, Quick, QuickControls2 and Widgets;
- SQLite3;
- Linux: PipeWire development package and, for X11 global hotkeys, X11/Xtst;
- Linux audio decoding: `libsndfile`.

```bash
cmake -S . -B build-desktop \
  -DPUFFY_BUILD_DESKTOP=ON \
  -DPUFFY_BUILD_TESTS=ON

cmake --build build-desktop --parallel
ctest --test-dir build-desktop --output-on-failure
```

Run the desktop binary:

```bash
./build-desktop/puffy_desktop
```

## Packaging

CPack can create a Linux archive:

```bash
cmake --build build-desktop --target package
```

Generated package formats depend on the CPack environment.

## Platform status

### Linux

The implemented target. PipeWire capture, monitoring output, virtual microphone and X11 global keyboard support are present.

Wayland global keyboard behavior depends on the compositor and permission model. puffy does not silently pretend that an X11 listener works under every Wayland session.

### Windows

WASAPI/MMDevice should provide physical capture and monitoring. A microphone endpoint visible to Discord requires a separate signed virtual audio driver. The application cannot create that system endpoint as an ordinary user process.

### macOS

Core Audio can provide capture and monitoring. A system-wide virtual input requires an Audio Server Plug-in or DriverKit component, plus signing, entitlements and packaging approval.

The Windows/macOS files in this repository are contracts/stubs, not finished virtual drivers. This is intentional; fake drivers are not a feature.

## Repository layout

```text
apps/desktop/       desktop entry points
core/audio/         audio formats, decoder, engine and ports
core/effects/       realtime microphone effects
core/hotkeys/       bindings, routing and listener contracts
core/library/       SQLite library and decoded cache
core/mixer/         realtime voice mixer
core/profiles/      profiles and playlists persistence
core/soundboard/    playback policies and service layer
platform/linux/     PipeWire and X11 implementations
platform/windows/   Windows backend contracts
platform/macos/     macOS backend contracts
ui/qml/             soft puffy interface
ui/qt/              Qt models and bridges
tests/              core tests and mocks
docs/               architecture and platform notes
```

## Privacy

puffy does not need to record typed text. Keyboard events are transient control signals used for hotkeys and Full Keyboard Mode.

The project does not intentionally store:

- typed text;
- keyboard history;
- sound-trigger history;
- microphone recordings;
- telemetry without explicit future consent.

## Roadmap

- finish device hotplug and precise latency reporting;
- add EQ, reverb and independent pitch shift;
- add drag-and-drop library management and waveform previews;
- finish playlist/profile editing UI;
- implement native Windows virtual audio driver;
- implement macOS virtual audio component;
- add Windows/macOS CI, signing and installers;
- run long-duration real-time audio stress tests.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Final warning

This is experimental audio software.

It may produce:

- silence;
- latency;
- feedback;
- too many airhorns;
- a sudden desire to rewrite the mixer.

Use headphones. Keep the master volume reasonable. Do not test the limiter with your expensive speakers at maximum volume.

> **Good luck. Stay soft. Keep the callback real-time safe.**
