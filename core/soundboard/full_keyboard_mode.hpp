#pragma once

#include <cstddef>
#include <random>
#include <vector>

namespace puffy::soundboard {

enum class FullKeyboardMode { Random, Sequential, Single };
enum class PlaylistEnd { Loop, Stop };

class FullKeyboardModeController final {
public:
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    void setMode(FullKeyboardMode mode) noexcept { mode_ = mode; }
    void setPlaylist(std::vector<int> soundIds);
    void setSingleSound(int soundId) noexcept { singleSoundId_ = soundId; }
    void setPlaylistEnd(PlaylistEnd end) noexcept { playlistEnd_ = end; }
    void setAvoidImmediateRepeats(bool value) noexcept { avoidImmediateRepeats_ = value; }
    void setIgnoredKey(int keyCode, bool ignored);
    void setIgnoreCtrl(bool value) noexcept { ignoreCtrl_ = value; }
    void setIgnoreShift(bool value) noexcept { ignoreShift_ = value; }
    void setIgnoreAlt(bool value) noexcept { ignoreAlt_ = value; }
    void setIgnoreSuper(bool value) noexcept { ignoreSuper_ = value; }
    [[nodiscard]] bool ignoresEvent(bool ctrl, bool shift, bool alt, bool super, int keyCode) const noexcept;
    void setTriggerOnRepeat(bool value) noexcept { triggerOnRepeat_ = value; }

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] int onKeyPress(int keyCode);
    [[nodiscard]] int onKeyPress(int keyCode, bool repeat);

private:
    bool enabled_{false};
    FullKeyboardMode mode_{FullKeyboardMode::Random};
    PlaylistEnd playlistEnd_{PlaylistEnd::Loop};
    bool avoidImmediateRepeats_{true};
    bool triggerOnRepeat_{false};
    bool ignoreCtrl_{false};
    bool ignoreShift_{false};
    bool ignoreAlt_{false};
    bool ignoreSuper_{false};
    int singleSoundId_{-1};
    int previousRandomId_{-1};
    std::size_t sequenceIndex_{0};
    std::vector<int> playlist_;
    std::vector<int> ignoredKeys_;
    std::mt19937 random_{std::random_device{}()};
};

} // namespace puffy::soundboard
