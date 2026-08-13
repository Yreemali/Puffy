#include "core/soundboard/playback_policy.hpp"

namespace puffy::soundboard {
namespace {

PlaybackCommand start(std::int64_t id, audio::OutputRoute route, float gain) {
    return {PlaybackCommandType::Start, id, route, gain};
}

PlaybackCommand stop(std::int64_t id, audio::OutputRoute route, float gain) {
    return {PlaybackCommandType::Stop, id, route, gain};
}

} // namespace

std::vector<PlaybackCommand> PlaybackPolicy::press(std::int64_t soundId, library::PlaybackMode mode,
                                                   audio::OutputRoute route, float gain, bool osRepeat) {
    auto& state = states_[soundId];
    const auto wasPlaying = state.soundPlaying;
    if (osRepeat && mode != library::PlaybackMode::Overlap) return {};
    if (mode == library::PlaybackMode::IgnoreIfPlaying && state.soundPlaying) return {};
    if (mode == library::PlaybackMode::Toggle && state.soundPlaying) {
        state.soundPlaying = false;
        state.down = true;
        return {stop(soundId, route, gain)};
    }
    if (mode == library::PlaybackMode::Hold && state.down) return {};
    state.down = true;
    state.soundPlaying = true;
    if (mode == library::PlaybackMode::Restart && wasPlaying) {
        return {stop(soundId, route, gain), start(soundId, route, gain)};
    }
    return {start(soundId, route, gain)};
}

std::vector<PlaybackCommand> PlaybackPolicy::release(std::int64_t soundId, library::PlaybackMode mode,
                                                     audio::OutputRoute route, float gain) {
    auto& state = states_[soundId];
    state.down = false;
    if (mode != library::PlaybackMode::Hold || !state.soundPlaying) return {};
    state.soundPlaying = false;
    return {stop(soundId, route, gain)};
}

void PlaybackPolicy::markFinished(std::int64_t soundId) noexcept {
    if (const auto it = states_.find(soundId); it != states_.end()) it->second.soundPlaying = false;
}

void PlaybackPolicy::stopAll() noexcept {
    for (auto& [unusedId, state] : states_) {
        state.down = false;
        state.soundPlaying = false;
    }
}

bool PlaybackPolicy::isPlaying(std::int64_t soundId) const noexcept {
    if (const auto it = states_.find(soundId); it != states_.end()) return it->second.soundPlaying;
    return false;
}

} // namespace puffy::soundboard
