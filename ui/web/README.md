# puffy Web UI

Modern React/TypeScript desktop interface for puffy. The renderer owns navigation,
library presentation, playlists, soundboard pads, player controls and settings UI.
It does not access PipeWire, CoreAudio, WASAPI, global keyboard APIs or the filesystem
directly. Those operations belong behind the native bridge in `src/native.ts`.

## Run

```bash
npm install
npm run dev
```

Production build:

```bash
npm run build
```

Tauri host build (requires Rust/Cargo and platform WebView dependencies):

```bash
npm run tauri:dev
npm run tauri:build
```

`npm run desktop` opens puffy in its own native desktop window. It does not open
the React UI as a browser tab: Tauri creates the application window and renders
the Vite page inside the platform WebView, similar to a Spotify desktop app.

## Native bridge

The browser fallback is intentionally safe demo behavior so the UI can be developed
without an installed audio server. The Tauri desktop host exposes the same
operations through Tauri commands or a Qt WebEngine/C++ IPC adapter:

- `playSound(id, route)`
- `stopAll()`
- `librarySnapshot()`
- `addSounds(paths)`
- `setSoundVolume(id, value)`
- `setMicrophoneGain(value)`
- `setMonitorMicrophone(enabled)`
- `setMasterVolume(value)`
- `stopAll()`

Audio processing remains in the existing C++ engine. The web layer should only send
bounded commands and receive immutable state snapshots/events.

## UX map

- Home: quick access, recent sounds and collection overview.
- Library: searchable list view with routing, hotkeys and duration.
- Playlists: playlist selection, add/remove sounds and ordering workflow.
- Soundboard: low-click large pads for live use.
- Microphone, Effects, Devices and Mixer: progressive-disclosure control centers.
- Hotkeys, History and Settings: reserved native-backed management screens.

The layout supports a collapsible sidebar, a permanent player bar, command palette
(`>` in the global search), reduced motion, keyboard-friendly controls and a dark
design system based on layered surfaces rather than glass effects.
