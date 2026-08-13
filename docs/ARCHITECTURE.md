# puffy — архитектура и feasibility analysis

## Решение

`puffy` строится как C++20 audio engine с Qt 6/QML UI и узкими платформенными адаптерами. Внутренний engine всегда разделяет два preallocated bus: `monitoring` и `virtual microphone`. Это позволяет для каждого soundboard-звука выбрать `Headphones only`, `Virtual microphone only` или `Both`; дополнительный флаг «не мониторить soundboard» не является обходным путём, а штатным маршрутом `Virtual microphone only`.

Платформенный virtual microphone не может быть одной полностью одинаковой библиотекой:

* Windows: WASAPI/MMDevice используются приложением для capture/playback, но для устройства, которое появится как input endpoint в Discord, нужен отдельный virtual audio driver. Driver и его installer — отдельные компоненты, устанавливаемые с elevation только на время установки.
* Linux: основной backend — PipeWire native client/filter node. Для PulseAudio-совместимых приложений используется PipeWire Pulse compatibility; на системах без PipeWire предусмотрен fallback с понятным статусом «virtual device unavailable», а не скрытая имитация.
* macOS: Core Audio обслуживает stream engine, а системный input endpoint требует отдельный Audio Server Plug-in/AudioDriverKit компонент и отдельный пакет установки. Основное приложение остаётся обычным user process.

Это соответствует официальному описанию Windows Audio Architecture, PipeWire graph/API и Apple Core Audio Audio Server Plug-in model: [Microsoft audio architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/windows-audio-architecture), [PipeWire API](https://docs.pipewire.org/devel/page_api.html), [PipeWire virtual source](https://docs.pipewire.org/page_pulse_module_virtual_source.html), [Apple Core Audio](https://developer.apple.com/documentation/coreaudio/).

## Технологии

* C++20, CMake 3.21+, clang-format/clang-tidy.
* Qt 6.5+ и Qt Quick/QML для desktop UI.
* Native audio: WASAPI (Windows), PipeWire (Linux), Core Audio (macOS).
* SQLite 3 для metadata и профилей; JSON экспорт — Qt JSON либо vendored single-header библиотека с проверенной лицензией.
* libsndfile для WAV/FLAC/OGG/AIFF/AU where supported; mpg123/minimp3 или FFmpeg-профиль для MP3/AAC/M4A/Opus после отдельной license review. Decode worker не вызывается из callback.
* Лицензия проекта: GPL-3.0-or-later. SQLite public domain; Qt LGPL 3 (использовать динамическую линковку и соблюдать LGPL); PipeWire MIT/LGPL-компоненты проверяются по используемым пакетам; Windows/macOS SDK — системные SDK с их условиями. Точные версии и notices фиксируются в `third_party/THIRD_PARTY_NOTICES.md` перед релизом.

## Слои

```text
QML UI / ViewModels
        |
Application services (commands, profiles, notifications)
        |
Core: library, soundboard, hotkeys, mixer, effects
        |
Audio ports: capture, monitoring output, virtual microphone output
        |
Platform adapters: WASAPI | PipeWire | Core Audio
        |
Optional privileged components: Windows driver | macOS plug-in
```

Core не знает Qt, OS key codes, device handles или database connections. UI получает immutable snapshots и посылает commands через application services.

## Потоки

```text
UI thread                  -> commands/snapshots
Global input thread        -> transient KeyEvent -> HotkeyManager
Decode workers             -> ready audio buffers/cache
Audio callback/thread      -> capture -> effects -> mixer -> 2 output buses
Database/file workers      -> SQLite, profiles, thumbnails, import/export
```

Audio callback запрещено: allocation, blocking mutex, filesystem/database I/O, synchronous logging и обращение к UI. Переходы состояний передаются через bounded SPSC/MPSC queues; buffers заранее выделяются.

## Audio pipeline

```text
physical capture (mono/stereo)
  -> channel conversion/resampler to 48 kHz float32
  -> microphone effects chain (gate -> EQ -> pitch -> compressor -> limiter)
  -> mixer input A
decoded sound voices (up to configured 16/32)
  -> per-voice gain/pitch/fades/loop
  -> mixer input B
  -> limiter / meter
  -> monitoring bus (headphones)
  -> virtual bus (system virtual microphone)
```

Microphone всегда идёт в virtual bus, если не включён explicit mute. Soundboard может идти в один или оба bus. Monitoring для микрофона и звуков регулируется раздельно; приложение показывает предупреждение при выборе speakers.

## Основные контракты

```cpp
class IVirtualMicrophone {
public:
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() noexcept = 0;
    virtual bool pushAudio(const float*, std::size_t, int, int) noexcept = 0;
    virtual ~IVirtualMicrophone() = default;
};
```

Дополнительные контракты: `IAudioCapture`, `IAudioOutput`, `IGlobalKeyboardListener`, `IAudioEffect`, `IAudioDecoder`, `ISoundRepository`. Платформенные реализации возвращают typed error/status и capability list.

## Data model

`Sound`: id, name, URI/path, duration, volume, category, tags, favorite, hotkey, playback mode, route, pitch, speed, fadeIn/fadeOut, loop. `Playlist`: id, name, ordered sound ids, loop/shuffle. `Profile`: version, sound references, playlists, bindings, effect chains, routes, volumes, keyboard configuration, theme. Paths экспортируются относительными только при включённой portable profile option; аудио не копируется без явного выбора.

## Hotkeys и privacy

`IGlobalKeyboardListener` получает только transient `KeyEvent`. По умолчанию listener не suppress-ит событие и не inject-ит замену. Full Keyboard Mode использует только `pressed`, modifiers, repeat и blacklist; текст, scan history и key log отсутствуют в модели данных и логах. На macOS accessibility/Input Monitoring permission, на Linux compositor/desktop ограничения и на Windows low-level hook объясняются в UI. «Trigger once» дедуплицирует repeat по физическому down/up состоянию.

Full Keyboard Mode — отдельный controller с режимами Random/Sequential/Single. Random поддерживает no-immediate-repeat; Sequential — Loop/Stop; ignored modifier/system keys фильтруются до выбора звука. Событие остаётся доступным системе, если backend не получил explicit user request на suppression (в puffy такой опции нет).

## Ошибки и восстановление

Все device backends публикуют `DeviceState`: Ready, Disconnected, PermissionDenied, Unsupported, Failed. При unplug engine переводит endpoint в muted/disconnected, сохраняет последние настройки и асинхронно пытается открыть default/previous device. Missing/corrupt file помечается в library и не ломает остальные voices. Hotkey conflict показывает конфликт до сохранения.

## Roadmap

1. Phase 1: текущий core contracts, mixer routing, tests; затем Qt shell, SQLite library, WAV/FLAC playback.
2. Phase 2: native device abstraction, global hotkeys, per-sound playback modes and stop commands.
3. Phase 3: capture, resampling, monitoring, meters and latency reporting.
4. Phase 4: PipeWire virtual node, Windows driver prototype/installer, macOS Audio Server Plug-in prototype.
5. Phase 5: Full Keyboard Mode, permission UX, tray and privacy documentation.
6. Phase 6: effects chain, profile serialization/import/export, theme editor.
7. Phase 7: stress tests, device hotplug, packaging/signing, reproducible CI builds and release audit.

## Packaging

Windows: MSIX/installer plus signed driver package; Linux: AppImage/Flatpak plus PipeWire integration; macOS: notarized `.app`/DMG plus separately approved audio component. Driver installation never runs from the audio callback or with permanent admin privileges.
