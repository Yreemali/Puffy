# Platform audio implementation status

## Linux

Implemented with PipeWire streams:

* physical capture;
* monitoring output;
* `OpenSoundboard Virtual Microphone` source node;
* float32/48 kHz realtime ring buffers.

## Windows

The application-side contract is prepared under `platform/windows`. WASAPI/MMDevice should provide physical capture and monitoring. A device selectable as an input in Discord requires a separately installed signed virtual audio driver; a normal user process cannot create that system endpoint. The driver must be implemented as a separate package and installed with elevation only during setup.

## macOS

The application-side contract is prepared under `platform/macos`. Core Audio should provide physical capture and monitoring. A system input endpoint requires an Audio Server Plug-in/DriverKit component and its own approval/signing/notarization flow. The normal application must remain unprivileged.

The Windows/macOS files are intentionally capability stubs until their native build environments and signing/install policies are available; they do not claim to provide a virtual microphone.
