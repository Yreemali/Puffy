# Native host plan

The React renderer is deliberately host-agnostic. A desktop host must inject
`window.__PUFFY_NATIVE__` before loading the bundle. The object is the only
supported route from the renderer to the native audio engine.

Required commands:

```ts
playSound(id: number, route?: 'headphones' | 'microphone' | 'both' | 'none'): Promise<void>
stopAll(): Promise<void>
importAudio(): Promise<string[]>
setMasterVolume(value: number): Promise<void>
savePlaylist(playlist: Playlist): Promise<void>
```

Required events for the production host:

- `library.snapshot` and `library.changed`;
- `playback.changed` and `playback.position`;
- `audio.devicesChanged`, `audio.stateChanged`, and `audio.meters`;
- `hotkeys.changed` and `hotkeys.conflict`;
- `toast.created`.

The native implementation should call the existing `SoundboardService`,
`AudioEngine`, `SoundLibrary`, `ProfileStore`, and platform device adapters.
It must never execute filesystem/database work on the realtime audio callback.

## Host options

The preferred host is now scaffolded under `ui/web/src-tauri`. Its commands are
deliberately bounded and validated. The current Rust facade stores state only;
the next native step is to link those commands to the existing C++
`SoundboardService`/`AudioEngine` through a small C ABI or a local native sidecar.
This keeps the Tauri renderer off the realtime audio path.

## Linux Wayland keyboard input

When `WAYLAND_DISPLAY` is present, puffy uses the Linux evdev backend instead of
X11/XRecord. It reads key events from `/dev/input/event*` without grabbing or
suppressing them, so the original key continues to reach the focused application.
This is the backend used by Full Keyboard Mode on Wayland.

The user must have read access to keyboard event devices, normally through the
`input` group. If access is unavailable, audio continues running and puffy reports
the permission error. Key history and typed text are never stored.

The XDG Global Shortcuts portal is suitable for registered ordinary shortcuts, but
cannot provide arbitrary every-key capture; evdev is therefore an explicit local
permission-based backend for Full Keyboard Mode.
