# Platform audio implementation status

## Linux

Implemented with PipeWire streams:

* physical capture;
* monitoring output;
* `OpenSoundboard Virtual Microphone` source node;
* float32/48 kHz realtime ring buffers.

## Windows

Implemented application-side pieces under `platform/windows`:

* MMDevice input/output enumeration with opaque endpoint IDs;
* event-driven shared-mode WASAPI capture and render;
* automatic PCM sample-rate/channel conversion by the Windows audio engine;
* bounded SPSC monitoring/transport buffers;
* passive `WH_KEYBOARD_LL` global keyboard listener with repeat detection.

A device selectable as an input in Discord still requires the separately installed,
signed virtual audio driver described in `drivers/windows_virtual_audio`. The app
writes to its render transport endpoint; the driver publishes the corresponding
capture endpoint. Driver installation is the only elevated operation.

## macOS

Implemented application-side pieces under `platform/macos`:

* Core Audio device enumeration by stable UID;
* AUHAL float32 capture and output;
* bounded SPSC monitoring/transport buffers;
* passive Quartz event tap with repeat/modifier handling and Input Monitoring UX;
* microphone usage description in the Tauri app bundle.

A system input endpoint requires the separately signed/notarized Audio Server
Plug-in or Driver Extension described in `drivers/macos_virtual_audio`. The normal
application remains unprivileged and reports degraded mode when the component is
not installed.

The Linux CI host validates portable code and contracts. Final platform binaries
must additionally be compiled and exercised on Windows with the Windows SDK/WDK
and on macOS with Xcode because those SDKs are not cross-platform redistributable.
