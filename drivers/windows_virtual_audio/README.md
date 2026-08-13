# Puffy Windows virtual microphone component

The desktop process cannot manufacture an MMDevice capture endpoint. The Windows
package therefore needs a separately signed Universal Audio Driver derived from
Microsoft's open SYSVAD sample.

The driver contract is deliberately small:

- public capture endpoint: `Puffy Virtual Microphone`;
- app-facing render endpoint: `Puffy Virtual Microphone Transport`;
- transport format: interleaved float32, 48 kHz, stereo;
- the WaveRT adapter forwards transport render frames to the public capture pin;
- silence is emitted on underrun; old audio is discarded rather than delayed;
- installation and removal use a componentized INF and require elevation;
- the desktop process continues to run as a normal user.

`WindowsVirtualMicrophone` discovers the transport endpoint through MMDevice and
writes the mixed stream with event-driven shared-mode WASAPI. Until the signed
driver package is installed, initialization returns false and Puffy runs in
degraded mode with local monitoring still available.

Do not ship Microsoft's sample certificates or enable test signing in a release.
Production packages require an organization-owned certificate and Microsoft
driver signing.
