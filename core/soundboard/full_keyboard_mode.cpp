#include "core/soundboard/full_keyboard_mode.hpp"

#include <algorithm>

namespace puffy::soundboard {

void FullKeyboardModeController::setPlaylist(std::vector<int> soundIds) {
    playlist_ = std::move(soundIds);
    sequenceIndex_ = 0;
}

void FullKeyboardModeController::setIgnoredKey(int keyCode, bool ignored) {
    const auto it = std::find(ignoredKeys_.begin(), ignoredKeys_.end(), keyCode);
    if (ignored && it == ignoredKeys_.end()) ignoredKeys_.push_back(keyCode);
    if (!ignored && it != ignoredKeys_.end()) ignoredKeys_.erase(it);
}

bool FullKeyboardModeController::ignoresEvent(bool ctrl, bool shift, bool alt, bool super, int keyCode) const noexcept {
    if ((ignoreCtrl_ && ctrl) || (ignoreShift_ && shift) || (ignoreAlt_ && alt) || (ignoreSuper_ && super)) return true;
    return std::find(ignoredKeys_.begin(), ignoredKeys_.end(), keyCode) != ignoredKeys_.end();
}

int FullKeyboardModeController::onKeyPress(int keyCode) {
    if (!enabled_ || std::find(ignoredKeys_.begin(), ignoredKeys_.end(), keyCode) != ignoredKeys_.end()) return -1;
    if (mode_ == FullKeyboardMode::Single) return singleSoundId_;
    if (playlist_.empty()) return -1;
    if (mode_ == FullKeyboardMode::Sequential) {
        if (sequenceIndex_ >= playlist_.size()) {
            if (playlistEnd_ == PlaylistEnd::Stop) return -1;
            sequenceIndex_ = 0;
        }
        return playlist_[sequenceIndex_++];
    }
    std::uniform_int_distribution<std::size_t> distribution(0, playlist_.size() - 1);
    auto selected = playlist_[distribution(random_)];
    if (avoidImmediateRepeats_ && playlist_.size() > 1) {
        while (selected == previousRandomId_) selected = playlist_[distribution(random_)];
    }
    previousRandomId_ = selected;
    return selected;
}

int FullKeyboardModeController::onKeyPress(int keyCode, bool repeat) {
    if (repeat && !triggerOnRepeat_) return -1;
    return onKeyPress(keyCode);
}

} // namespace puffy::soundboard
