#pragma once

#include "core/library/sound_library.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace puffy::soundboard {

enum class PlaybackCommandType : std::uint8_t {
    Start,
    Stop,
};

struct PlaybackCommand {
    PlaybackCommandType type{PlaybackCommandType::Start};
    std::int64_t soundId{0};
    audio::OutputRoute route{audio::OutputRoute::Both};
    float gain{1.0F};
    bool loop{false};
    float speed{1.0F};
    std::size_t fadeInFrames{0};
    std::size_t fadeOutFrames{0};
};

struct PlaybackKeyState {
    bool down{false};
    bool soundPlaying{false};
};

class PlaybackPolicy final {
public:
    [[nodiscard]] std::vector<PlaybackCommand> press(
        std::int64_t soundId,
        library::PlaybackMode mode,
        audio::OutputRoute route,
        float gain,
        bool osRepeat = false);

    [[nodiscard]] std::vector<PlaybackCommand> release(
        std::int64_t soundId,
        library::PlaybackMode mode,
        audio::OutputRoute route,
        float gain);

    void markFinished(std::int64_t soundId) noexcept;
    void stopAll() noexcept;
    [[nodiscard]] bool isPlaying(std::int64_t soundId) const noexcept;

private:
    std::unordered_map<std::int64_t, PlaybackKeyState> states_;
};

} // namespace puffy::soundboard
