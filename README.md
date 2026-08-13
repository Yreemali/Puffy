# puffy

Open-source cross-platform soundboard with microphone mixing and a system virtual microphone. The UI direction is intentionally soft and cute: rounded cards, pastel defaults, accessible contrast, and a theme editor for custom palettes.

## Status

Early foundation. The first checked-in module contains allocation-free mixer routing, hotkey dispatch, Full Keyboard Mode selection, and tests. System audio devices, Qt UI, database, and privileged virtual-device components are implemented incrementally according to [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Build

```sh
cmake -S . -B build -DPUFFY_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The Qt shell is opt-in because Qt 6 is not required to build/test the real-time-safe core:

```sh
cmake -S . -B build -DPUFFY_BUILD_DESKTOP=ON
```

## Product-specific routing

Every sound has an output route: headphones only, virtual microphone only, or both. Thus a sound can be sent to Discord through the virtual microphone without being repeated in the user's headphones. Microphone monitoring remains independently controllable.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
