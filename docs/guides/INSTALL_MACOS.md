# Installing Puffy on macOS

Puffy targets macOS 12 or newer and uses Core Audio/AUHAL. A signed and
notarized DMG is the normal distribution format.

## Install a release build

1. Open the notarized `Puffy_*.dmg`.
2. Drag `Puffy.app` to Applications.
3. Launch Puffy from Applications.
4. Grant Microphone permission when requested.
5. Grant Input Monitoring under System Settings → Privacy & Security when
   global hotkeys or Full Keyboard Mode are needed, then restart Puffy.

Do not bypass Gatekeeper for files from an untrusted source. Unsigned CI bundles
are intended for development and will not provide normal release trust.

## Build locally

Install Xcode Command Line Tools, CMake, Rust stable and Node.js 22, then:

```bash
brew install sqlite3 libsndfile
export CMAKE_PREFIX_PATH="$(brew --prefix sqlite3):$(brew --prefix libsndfile)"
cd ui/web
npm ci
npm run tauri:build -- --bundles app,dmg
```

The wrapper copies the native dylib dependencies into
`Puffy.app/Contents/Frameworks` and rewrites their references to relocatable
`@rpath` paths.

## Virtual microphone

The `.app` alone cannot publish a Core Audio input endpoint. Production releases
need a separately signed/notarized Audio Server Plug-in or DriverKit extension.
Installation and approval follow Apple's driver-extension security model and
may require logout or reboot depending on the component design.

Until that component exists and is installed, Puffy still supports local
playback, physical microphone capture and monitoring, but no other application
will see `Puffy Virtual Microphone`.

## Application data

Puffy stores its database in `~/Library/Application Support/Puffy`.

## References

- [Tauri macOS application bundle](https://v2.tauri.app/distribute/macos-application-bundle/)
- [Tauri DMG distribution](https://v2.tauri.app/distribute/dmg/)
- [Apple: open apps safely](https://support.apple.com/102445)
