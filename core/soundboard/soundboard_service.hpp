#pragma once

#include "core/audio/audio_engine.hpp"
#include "core/library/sound_cache.hpp"
#include "core/library/sound_library.hpp"
#include "core/soundboard/playback_policy.hpp"

#include <cstdint>
#include <unordered_set>

namespace puffy::soundboard {

class SoundboardService final {
public:
    SoundboardService(library::SoundLibrary& library,
                      library::SoundCache& cache,
                      const audio::IAudioDecoder& decoder,
                      audio::AudioEngine& engine);

    // Decode and register a sound before AudioEngine::start().
    bool prepareSound(std::int64_t soundId);
    bool prepareAll();

    // Called by ordinary hotkeys or Full Keyboard Mode. The service does not
    // suppress the OS key event; it only creates playback commands.
    bool trigger(std::int64_t soundId, bool pressed, bool osRepeat = false);
    void stopAll() noexcept;

    [[nodiscard]] const std::string& lastError() const noexcept { return lastError_; }

private:
    bool execute(const PlaybackCommand& command) noexcept;
    bool setError(std::string message);

    library::SoundLibrary& library_;
    library::SoundCache& cache_;
    const audio::IAudioDecoder& decoder_;
    audio::AudioEngine& engine_;
    PlaybackPolicy policy_;
    std::unordered_set<std::int64_t> preparedSounds_;
    std::string lastError_;
};

} // namespace puffy::soundboard
