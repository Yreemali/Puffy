#include "core/soundboard/soundboard_service.hpp"

#include <utility>

namespace puffy::soundboard {

SoundboardService::SoundboardService(library::SoundLibrary& library,
                                     library::SoundCache& cache,
                                     const audio::IAudioDecoder& decoder,
                                     audio::AudioEngine& engine)
    : library_(library), cache_(cache), decoder_(decoder), engine_(engine) {}

bool SoundboardService::setError(std::string message) {
    lastError_ = std::move(message);
    return false;
}

bool SoundboardService::prepareSound(std::int64_t soundId) {
    if (preparedSounds_.contains(soundId)) return true;
    const auto sound = library_.find(soundId);
    if (!sound) return setError("Sound not found");
    std::string error;
    const auto decoded = cache_.load(sound->filePath, decoder_, error);
    if (!decoded) return setError(error.empty() ? "Unable to decode sound" : error);
    const auto sampleRate = static_cast<std::size_t>(decoded->sampleRate);
    const auto fadeInFrames = static_cast<std::size_t>(sound->fadeInSeconds * static_cast<float>(sampleRate));
    const auto fadeOutFrames = static_cast<std::size_t>(sound->fadeOutSeconds * static_cast<float>(sampleRate));
    if (!engine_.registerSound(soundId, decoded, sound->loop, fadeInFrames, fadeOutFrames, sound->speed)) return setError("Unable to register sound in audio engine");
    preparedSounds_.insert(soundId);
    lastError_.clear();
    return true;
}

bool SoundboardService::prepareAll() {
    for (const auto& sound : library_.all()) {
        if (!prepareSound(sound.id)) return false;
    }
    return true;
}

bool SoundboardService::execute(const PlaybackCommand& command) noexcept {
    if (command.type == PlaybackCommandType::Start)
        return engine_.playSound(command.soundId, command.route, command.gain, command.loop, command.speed, command.fadeInFrames, command.fadeOutFrames);
    return engine_.stopSound(command.soundId);
}

bool SoundboardService::trigger(std::int64_t soundId, bool pressed, bool osRepeat) {
    const auto sound = library_.find(soundId);
    if (!sound) return setError("Sound not found");
    if (!prepareSound(soundId)) return false;
    const auto commands = pressed
        ? policy_.press(soundId, sound->playbackMode, sound->route, sound->volume, osRepeat)
        : policy_.release(soundId, sound->playbackMode, sound->route, sound->volume);
    for (auto command : commands) {
        command.loop = sound->loop;
        command.speed = sound->speed;
        command.fadeInFrames = static_cast<std::size_t>(sound->fadeInSeconds * static_cast<float>(48000));
        command.fadeOutFrames = static_cast<std::size_t>(sound->fadeOutSeconds * static_cast<float>(48000));
        if (!execute(command)) return setError("Audio command queue is full or sound is unavailable");
    }
    return true;
}

void SoundboardService::stopAll() noexcept {
    policy_.stopAll();
    engine_.stopAllSounds();
}

} // namespace puffy::soundboard
