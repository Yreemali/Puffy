# Puffy Web UI architecture

The desktop frontend is a React/Vite/TypeScript renderer under `ui/web`.

```text
React renderer
  ├── AppShell / Sidebar / TopBar / PlayerBar
  ├── Library / Soundboard / Playlists
  ├── Microphone / Effects / Mixer / Devices
  └── NativeBridge (IPC boundary)
                    │ bounded commands + state events
                    ▼
              C++ audio engine
                    │
        PipeWire / WASAPI / CoreAudio
```

The renderer must never perform audio decoding, device enumeration, global keyboard
capture, filesystem scanning or realtime processing. The native side should publish
state snapshots such as `audioState`, `devices`, `sounds`, `playback`, `meters` and
`notifications`. Commands are explicit and idempotent where possible.

## Required IPC events

The first production host integration should provide:

- `library.snapshot`, `library.changed`, `library.importProgress`;
- `playback.changed`, `playback.position`, `playback.queueChanged`;
- `audio.devicesChanged`, `audio.stateChanged`, `audio.meters`;
- `hotkeys.changed`, `hotkeys.conflict`;
- `toast.created` for recoverable errors and device disconnects.

## Why web UI, native audio

React gives puffy a maintainable information architecture and polished desktop UX.
The C++ engine remains responsible for realtime guarantees and platform audio APIs;
the renderer is never placed on the audio callback path.
