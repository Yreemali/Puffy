#include "native/puffy_native.h"

#include "core/audio/audio_engine.hpp"
#include "core/library/sound_cache.hpp"
#include "core/library/sound_library.hpp"
#include "core/profiles/profile_store.hpp"
#include "core/soundboard/soundboard_service.hpp"
#include "core/hotkeys/hotkey_router.hpp"
#include "core/effects/audio_effect.hpp"

#ifdef PUFFY_HAS_PIPEWIRE
#include "platform/linux/pipewire_capture.hpp"
#include "platform/linux/pipewire_output.hpp"
#include "platform/linux/pipewire_virtual_microphone.hpp"
#endif
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS)
#include "platform/linux/x11_global_keyboard_listener.hpp"
#endif
#if defined(PUFFY_HAS_EVDEV)
#include "platform/linux/evdev_global_keyboard_listener.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <sstream>
#include <cstdlib>
#include <unordered_map>

struct puffy_native_context {
    explicit puffy_native_context(const char* path)
        : library(path ? path : "puffy.sqlite"), profiles(path ? path : "puffy.sqlite"),
          engine({{48000, 2}, 2048, 128, {2048, 2, 32}}), service(library, cache, decoder, engine)
          {
              engine.addMicrophoneEffect(noiseGate);
              engine.addMicrophoneEffect(compressor);
              engine.addMicrophoneEffect(limiter);
          }

    puffy::library::SoundLibrary library;
    puffy::profiles::ProfileStore profiles;
    puffy::library::SoundCache cache;
    puffy::audio::SndFileDecoder decoder;
    puffy::audio::AudioEngine engine;
    puffy::soundboard::SoundboardService service;
    puffy::effects::NoiseGate noiseGate;
    puffy::effects::Compressor compressor;
    puffy::effects::Limiter limiter;
#ifdef PUFFY_HAS_PIPEWIRE
    puffy::platform::linux::PipeWireCapture capture;
    puffy::platform::linux::PipeWireOutput output;
    puffy::platform::linux::PipeWireVirtualMicrophone microphone;
#endif
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    std::unique_ptr<puffy::hotkeys::IGlobalKeyboardListener> keyboardListener;
    std::unique_ptr<puffy::hotkeys::HotkeyRouter> hotkeys;
#endif
    std::string error;
    std::string snapshot;
    std::unordered_map<std::int64_t, std::size_t> playlistCursors;
    bool started{false};
};

void configureHotkeys(puffy_native_context& context) {
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    if (!context.hotkeys) return;
    context.hotkeys->clearBindings();
    for (const auto& sound : context.library.all()) {
        if (!sound.hotkey.empty()) context.hotkeys->bindSoundText(sound.hotkey, sound.id);
    }
    for (const auto& playlist : context.profiles.playlists()) {
        const auto playlistId = playlist.id;
        const auto mode = playlist.playbackMode;
        if (!playlist.hotkey.empty()) context.hotkeys->bindActionText(playlist.hotkey, [&context, playlistId, mode](const auto& event) {
            if (!event.pressed) return;
            const auto playlists = context.profiles.playlists();
            const auto it = std::find_if(playlists.begin(), playlists.end(), [playlistId](const auto& item) { return item.id == playlistId; });
            if (it == playlists.end() || it->soundIds.empty()) return;
            auto& cursor = context.playlistCursors[playlistId];
            std::size_t index = cursor % it->soundIds.size();
            if (mode == 1) index = static_cast<std::size_t>(std::rand()) % it->soundIds.size();
            context.service.trigger(it->soundIds[index], true, false);
            cursor = index + 1;
        });
        if (!playlist.nextHotkey.empty()) context.hotkeys->bindActionText(playlist.nextHotkey, [&context, playlistId](const auto& event) {
            if (!event.pressed) return;
            const auto playlists = context.profiles.playlists();
            const auto it = std::find_if(playlists.begin(), playlists.end(), [playlistId](const auto& item) { return item.id == playlistId; });
            if (it == playlists.end() || it->soundIds.empty()) return;
            auto& cursor = context.playlistCursors[playlistId];
            const auto index = cursor % it->soundIds.size();
            context.service.trigger(it->soundIds[index], true, false);
            cursor = index + 1;
        });
    }
#else
    (void)context;
#endif
}

namespace {
std::string json(const std::string& value) {
    std::string result = "\"";
    for (const auto character : value) {
        if (character == '\\' || character == '"') result += '\\';
        result += character;
    }
    return result + "\"";
}

void refreshSnapshot(puffy_native_context& context) {
    std::ostringstream output;
    output << "{\"sounds\":[";
    const auto sounds = context.library.all();
    for (std::size_t index = 0; index < sounds.size(); ++index) {
        if (index != 0) output << ',';
        const auto& sound = sounds[index];
        output << "{\"id\":" << sound.id << ",\"name\":" << json(sound.name)
               << ",\"duration\":" << sound.durationSeconds << ",\"volume\":" << sound.volume
               << ",\"hotkey\":" << json(sound.hotkey) << ",\"playbackMode\":" << static_cast<int>(sound.playbackMode) << ",\"route\":\""
               << (sound.route == puffy::audio::OutputRoute::Both ? "both" : sound.route == puffy::audio::OutputRoute::Headphones ? "headphones" : sound.route == puffy::audio::OutputRoute::VirtualMicrophone ? "microphone" : "none")
               << "\",\"favorite\":" << (sound.favorite ? "true" : "false") << ",\"color\":\"#6f83d8\",\"playlistIds\":[";
        bool firstPlaylist = true;
        for (const auto& candidate : context.profiles.playlists()) {
            if (std::find(candidate.soundIds.begin(), candidate.soundIds.end(), sound.id) == candidate.soundIds.end()) continue;
            if (!firstPlaylist) output << ',';
            output << candidate.id; firstPlaylist = false;
        }
        output << "]}";
    }
    output << "],\"playlists\":[";
    const auto playlists = context.profiles.playlists();
    for (std::size_t index = 0; index < playlists.size(); ++index) {
        if (index != 0) output << ',';
        const auto& playlist = playlists[index];
        output << "{\"id\":" << playlist.id << ",\"name\":" << json(playlist.name)
               << ",\"description\":\"\",\"color\":\"#6f83d8\",\"hotkey\":" << json(playlist.hotkey)
               << ",\"nextHotkey\":" << json(playlist.nextHotkey) << ",\"playbackMode\":" << playlist.playbackMode << ",\"soundIds\":[";
        for (std::size_t item = 0; item < playlist.soundIds.size(); ++item) {
            if (item != 0) output << ',';
            output << playlist.soundIds[item];
        }
        output << "]}";
    }
    output << "]}";
    context.snapshot = output.str();
}
}

extern "C" puffy_native_context* puffy_native_create(const char* path) {
    auto context = std::make_unique<puffy_native_context>(path);
    if (!context->library.open() || !context->profiles.open()) {
        context->error = context->library.lastError().empty() ? context->profiles.lastError() : context->library.lastError();
        return context.release();
    }
    context->service.prepareAll();
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    if (std::getenv("WAYLAND_DISPLAY") != nullptr) context->keyboardListener = std::make_unique<puffy::platform::linux::EvdevGlobalKeyboardListener>();
#ifdef PUFFY_HAS_X11_GLOBAL_HOTKEYS
    else context->keyboardListener = std::make_unique<puffy::platform::linux::X11GlobalKeyboardListener>();
#else
    else context->keyboardListener = std::make_unique<puffy::platform::linux::EvdevGlobalKeyboardListener>();
#endif
    context->hotkeys = std::make_unique<puffy::hotkeys::HotkeyRouter>(*context->keyboardListener, context->service);
#endif
    const auto existingPlaylists = context->profiles.playlists();
    const auto hasAllSounds = std::any_of(existingPlaylists.begin(), existingPlaylists.end(), [](const auto& playlist) { return playlist.name == "All sounds"; });
    if (!hasAllSounds) {
        puffy::profiles::Playlist allSounds;
        allSounds.name = "All sounds";
        for (const auto& sound : context->library.all()) allSounds.soundIds.push_back(sound.id);
        context->profiles.savePlaylist(allSounds);
    } else {
        const auto all = std::find_if(existingPlaylists.begin(), existingPlaylists.end(), [](const auto& playlist) { return playlist.name == "All sounds"; });
        auto allSounds = *all;
        for (const auto& sound : context->library.all()) if (std::find(allSounds.soundIds.begin(), allSounds.soundIds.end(), sound.id) == allSounds.soundIds.end()) allSounds.soundIds.push_back(sound.id);
        context->profiles.replacePlaylistItems(allSounds);
    }
    refreshSnapshot(*context);
    return context.release();
}

extern "C" void puffy_native_destroy(puffy_native_context* context) {
    if (!context) return;
    puffy_native_stop(context);
    delete context;
}

extern "C" int puffy_native_start(puffy_native_context* context) {
    if (!context) return 0;
#ifdef PUFFY_HAS_PIPEWIRE
    if (context->started) return 1;
    if (!context->engine.start(context->capture, context->output, &context->microphone)) {
        context->error = "Unable to start PipeWire audio graph";
        return 0;
    }
    context->started = true;
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    configureHotkeys(*context);
    if (!context->hotkeys->start()) context->error = context->hotkeys->lastError();
#endif
    return 1;
#else
    context->error = "No platform audio backend was compiled";
    return 0;
#endif
}

extern "C" void puffy_native_stop(puffy_native_context* context) {
    if (!context) return;
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    if (context->hotkeys) context->hotkeys->stop();
#endif
    context->engine.stop();
    context->started = false;
}

extern "C" int puffy_native_play_sound(puffy_native_context* context, int64_t id, puffy_native_route route) {
    if (!context || !context->started) return 0;
    (void)route; // Per-sound routing is persisted in SoundLibrary and applied by the service.
    if (!context->service.trigger(id, true)) {
        context->error = context->service.lastError();
        return 0;
    }
    return 1;
}

extern "C" int puffy_native_stop_all(puffy_native_context* context) {
    if (!context) return 0;
    context->service.stopAll();
    return 1;
}

extern "C" int puffy_native_set_master_volume(puffy_native_context* context, float value) {
    if (!context || !std::isfinite(value) || value < 0.0F || value > 2.0F) return 0;
    context->engine.setSoundboardGain(value);
    return 1;
}

extern "C" int puffy_native_set_soundboard_volume(puffy_native_context* context, float value) {
    if (!context || !std::isfinite(value) || value < 0.0F || value > 2.0F) return 0;
    context->engine.setSoundboardGain(value); return 1;
}

extern "C" int puffy_native_set_monitoring_volume(puffy_native_context* context, float value) {
    if (!context || !std::isfinite(value) || value < 0.0F || value > 2.0F) return 0;
    context->engine.setMonitoringGain(value); return 1;
}

extern "C" int puffy_native_set_virtual_output_volume(puffy_native_context* context, float value) {
    if (!context || !std::isfinite(value) || value < 0.0F || value > 2.0F) return 0;
    context->engine.setVirtualOutputGain(value); return 1;
}

extern "C" int puffy_native_set_monitoring_muted(puffy_native_context* context, int muted) {
    if (!context) return 0; context->engine.setMonitoringMuted(muted != 0); return 1;
}

extern "C" int puffy_native_set_virtual_microphone_muted(puffy_native_context* context, int muted) {
    if (!context) return 0; context->engine.setVirtualMicrophoneMuted(muted != 0); return 1;
}

extern "C" const char* puffy_native_last_error(const puffy_native_context* context) {
    return context ? context->error.c_str() : "Native context is null";
}

extern "C" const char* puffy_native_library_snapshot(const puffy_native_context* context) {
    return context ? context->snapshot.c_str() : "{\"sounds\":[],\"playlists\":[]}";
}

extern "C" int puffy_native_add_sound(puffy_native_context* context, const char* path) {
    if (!context || !path || !*path) return 0;
    puffy::audio::SndFileDecoder decoder;
    std::string error;
    const auto decoded = decoder.decode(path, error);
    if (!decoded) { context->error = error; return 0; }
    puffy::library::Sound sound;
    sound.name = std::filesystem::path(path).stem().string();
    sound.filePath = path;
    sound.durationSeconds = static_cast<double>(decoded->frames()) / decoded->sampleRate;
    if (!context->library.add(sound) || !context->service.prepareSound(sound.id)) {
        context->error = context->library.lastError().empty() ? context->service.lastError() : context->library.lastError();
        return 0;
    }
    for (const auto& playlist : context->profiles.playlists()) {
        if (playlist.name != "All sounds") continue;
        auto updated = playlist;
        updated.soundIds.push_back(sound.id);
        context->profiles.replacePlaylistItems(updated);
        break;
    }
    refreshSnapshot(*context);
    return 1;
}

extern "C" int puffy_native_set_sound_volume(puffy_native_context* context, int64_t id, float value) {
    if (!context || !std::isfinite(value) || value < 0.0F || value > 2.0F) return 0;
    const auto sound = context->library.find(id);
    if (!sound) return 0;
    auto updated = *sound; updated.volume = value;
    if (!context->library.update(updated)) { context->error = context->library.lastError(); return 0; }
    refreshSnapshot(*context); return 1;
}

extern "C" int puffy_native_set_sound_route(puffy_native_context* context, int64_t id, puffy_native_route route) {
    if (!context) return 0;
    const auto sound = context->library.find(id);
    if (!sound) { context->error = "Sound not found"; return 0; }
    auto updated = *sound;
    updated.route = static_cast<puffy::audio::OutputRoute>(route);
    if (!context->library.update(updated)) { context->error = context->library.lastError(); return 0; }
    refreshSnapshot(*context); return 1;
}

extern "C" int puffy_native_set_sound_playback_mode(puffy_native_context* context, int64_t id, int mode) {
    if (!context || mode < 0 || mode > 4) return 0;
    const auto sound = context->library.find(id);
    if (!sound) { context->error = "Sound not found"; return 0; }
    auto updated = *sound;
    updated.playbackMode = static_cast<puffy::library::PlaybackMode>(mode);
    if (!context->library.update(updated)) { context->error = context->library.lastError(); return 0; }
    refreshSnapshot(*context); return 1;
}

extern "C" int puffy_native_set_sound_hotkey(puffy_native_context* context, int64_t id, const char* hotkey) {
    if (!context || !hotkey) return 0;
    const auto sound = context->library.find(id);
    if (!sound) { context->error = "Sound not found"; return 0; }
    auto updated = *sound; updated.hotkey = hotkey;
    if (!context->library.update(updated)) { context->error = context->library.lastError(); return 0; }
    refreshSnapshot(*context);
    if (context->started) configureHotkeys(*context);
    return 1;
}

extern "C" int puffy_native_set_full_keyboard(puffy_native_context* context, int enabled, int mode,
                                                int64_t playlistId, int64_t singleSoundId,
                                                int avoidRepeats, int triggerOnRepeat,
                                                int ignoreCtrl, int ignoreShift, int ignoreAlt,
                                                int ignoreSuper) {
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    if (!context || mode < 0 || mode > 2) return 0;
    context->hotkeys->setFullKeyboardEnabled(enabled != 0);
    context->hotkeys->setFullKeyboardMode(static_cast<puffy::soundboard::FullKeyboardMode>(mode));
    context->hotkeys->setFullKeyboardAvoidImmediateRepeats(avoidRepeats != 0);
    context->hotkeys->setFullKeyboardTriggerOnRepeat(triggerOnRepeat != 0);
    context->hotkeys->setFullKeyboardIgnoreCtrl(ignoreCtrl != 0);
    context->hotkeys->setFullKeyboardIgnoreShift(ignoreShift != 0);
    context->hotkeys->setFullKeyboardIgnoreAlt(ignoreAlt != 0);
    context->hotkeys->setFullKeyboardIgnoreSuper(ignoreSuper != 0);
    context->hotkeys->setFullKeyboardSingleSound(static_cast<int>(singleSoundId));
    const auto playlists = context->profiles.playlists();
    const auto it = std::find_if(playlists.begin(), playlists.end(), [playlistId](const auto& item) { return item.id == playlistId; });
    context->hotkeys->setFullKeyboardPlaylist(it == playlists.end() ? std::vector<int>{} : [&] { std::vector<int> ids; for (const auto id : it->soundIds) ids.push_back(static_cast<int>(id)); return ids; }());
    return 1;
#else
    (void)context; (void)enabled; (void)mode; (void)playlistId; (void)singleSoundId; (void)avoidRepeats; (void)triggerOnRepeat; (void)ignoreCtrl; (void)ignoreShift; (void)ignoreAlt; (void)ignoreSuper;
    return 0;
#endif
}

extern "C" int64_t puffy_native_create_playlist(puffy_native_context* context, const char* name) {
    if (!context || !name || !*name) return 0;
    puffy::profiles::Playlist playlist; playlist.name = name;
    if (!context->profiles.savePlaylist(playlist)) { context->error = context->profiles.lastError(); return 0; }
    refreshSnapshot(*context); return playlist.id;
}

extern "C" int puffy_native_add_sound_to_playlist(puffy_native_context* context, int64_t playlistId, int64_t soundId) {
    if (!context) return 0;
    auto playlists = context->profiles.playlists();
    const auto it = std::find_if(playlists.begin(), playlists.end(), [playlistId](const auto& item) { return item.id == playlistId; });
    if (it == playlists.end()) { context->error = "Playlist not found"; return 0; }
    auto playlist = *it;
    if (std::find(playlist.soundIds.begin(), playlist.soundIds.end(), soundId) == playlist.soundIds.end()) playlist.soundIds.push_back(soundId);
    if (!context->profiles.replacePlaylistItems(playlist)) { context->error = context->profiles.lastError(); return 0; }
    refreshSnapshot(*context); return 1;
}

extern "C" int puffy_native_set_playlist_hotkey(puffy_native_context* context, int64_t playlistId, const char* hotkey, int mode, const char* nextHotkey) {
#if defined(PUFFY_HAS_X11_GLOBAL_HOTKEYS) || defined(PUFFY_HAS_EVDEV)
    if (!context || !hotkey || mode < 0 || mode > 1) return 0;
    auto playlists = context->profiles.playlists();
    const auto it = std::find_if(playlists.begin(), playlists.end(), [playlistId](const auto& item) { return item.id == playlistId; });
    if (it == playlists.end()) { context->error = "Playlist not found"; return 0; }
    auto playlist = *it;
    playlist.hotkey = hotkey;
    playlist.nextHotkey = nextHotkey ? nextHotkey : "";
    playlist.playbackMode = mode;
    if (!context->profiles.updatePlaylistHotkeys(playlist)) { context->error = context->profiles.lastError(); return 0; }
    context->playlistCursors[playlistId] = 0;
    if (context->started) configureHotkeys(*context);
    return 1;
#else
    (void)context; (void)playlistId; (void)hotkey; (void)mode; (void)nextHotkey;
    return 0;
#endif
}

extern "C" int puffy_native_set_microphone_gain(puffy_native_context* context, float value) {
    if (!context || !std::isfinite(value) || value < 0.0F || value > 4.0F) return 0;
    context->engine.setMicrophoneGain(value); return 1;
}

extern "C" int puffy_native_set_monitor_microphone(puffy_native_context* context, int enabled) {
    if (!context) return 0;
    context->engine.setMonitorMicrophone(enabled != 0); return 1;
}

extern "C" int puffy_native_set_effect_parameter(puffy_native_context* context, const char* effect, const char* parameter, float value) {
    if (!context || !effect || !parameter || !std::isfinite(value)) return 0;
    const std::string effectName(effect);
    const std::string parameterName(parameter);
    if (effectName == "noise_gate" && parameterName == "threshold") { context->noiseGate.setThreshold(std::clamp(value, 0.0F, 1.0F)); return 1; }
    if (effectName == "compressor" && parameterName == "ratio") { context->compressor.setRatio(std::clamp(value, 1.0F, 20.0F)); return 1; }
    if (effectName == "compressor" && parameterName == "threshold") { context->compressor.setThreshold(std::clamp(value, 0.01F, 1.0F)); return 1; }
    if (effectName == "limiter" && parameterName == "ceiling") { context->limiter.setCeiling(std::clamp(value, 0.1F, 1.0F)); return 1; }
    context->error = "Unknown effect parameter";
    return 0;
}
