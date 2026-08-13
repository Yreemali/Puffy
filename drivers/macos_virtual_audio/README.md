# Puffy macOS virtual microphone component

The system-wide input endpoint is a separate Audio Server Plug-in / Driver
Extension based on Apple's official virtual-audio-device sample.

The component contract is:

- public input name: `Puffy Virtual Microphone`;
- app-facing output name: `Puffy Virtual Microphone Transport`;
- stable transport UID: `dev.puffy.virtual-audio.transport`;
- transport format: interleaved float32, 48 kHz, stereo;
- the plug-in copies transport output frames into the public input ring;
- silence is returned on underrun and stale frames are discarded;
- installation is handled by a signed/notarized package, separately from the
  unprivileged Puffy application.

`MacOSVirtualMicrophone` discovers the transport device by UID and feeds it through
AUHAL. Without the installed component Puffy remains usable for local playback and
microphone monitoring, but no virtual input is advertised to other applications.

The app bundle must contain `NSMicrophoneUsageDescription`; global keyboard mode
also requires the user to grant Input Monitoring permission.
