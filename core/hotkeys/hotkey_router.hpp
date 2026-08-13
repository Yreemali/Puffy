#pragma once

#include "core/hotkeys/global_keyboard_listener.hpp"
#include "core/hotkeys/hotkey_manager.hpp"
#include "core/soundboard/soundboard_service.hpp"
#include "core/soundboard/full_keyboard_mode.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace puffy::hotkeys {

class HotkeyRouter final {
public:
    HotkeyRouter(IGlobalKeyboardListener& listener, soundboard::SoundboardService& service);
    ~HotkeyRouter();

    HotkeyRouter(const HotkeyRouter&) = delete;
    HotkeyRouter& operator=(const HotkeyRouter&) = delete;

    bool start();
    void stop() noexcept;
    bool bindSound(Hotkey hotkey, std::int64_t soundId);
    bool bindSoundText(std::string_view text, std::int64_t soundId);
    void setFullKeyboardEnabled(bool enabled) noexcept { fullKeyboard_.setEnabled(enabled); }
    void setFullKeyboardMode(soundboard::FullKeyboardMode mode) noexcept { fullKeyboard_.setMode(mode); }
    void setFullKeyboardPlaylist(std::vector<int> soundIds) { fullKeyboard_.setPlaylist(std::move(soundIds)); }
    void setFullKeyboardSingleSound(int soundId) noexcept { fullKeyboard_.setSingleSound(soundId); }
    void setFullKeyboardIgnoreCtrl(bool value) noexcept { fullKeyboard_.setIgnoreCtrl(value); }
    void setFullKeyboardIgnoreShift(bool value) noexcept { fullKeyboard_.setIgnoreShift(value); }
    void setFullKeyboardIgnoreAlt(bool value) noexcept { fullKeyboard_.setIgnoreAlt(value); }
    void setFullKeyboardIgnoreSuper(bool value) noexcept { fullKeyboard_.setIgnoreSuper(value); }
    void setFullKeyboardIgnoredKey(int keyCode, bool ignored) { fullKeyboard_.setIgnoredKey(keyCode, ignored); }
    void setFullKeyboardTriggerOnRepeat(bool enabled) noexcept { fullKeyboard_.setTriggerOnRepeat(enabled); }
    void setFullKeyboardAvoidImmediateRepeats(bool enabled) noexcept { fullKeyboard_.setAvoidImmediateRepeats(enabled); }
    void clearBindings();
    [[nodiscard]] ListenerStatus status() const noexcept { return status_; }
    [[nodiscard]] const std::string& lastError() const noexcept { return error_; }

private:
    IGlobalKeyboardListener& listener_;
    soundboard::SoundboardService& service_;
    HotkeyManager manager_;
    soundboard::FullKeyboardModeController fullKeyboard_;
    ListenerStatus status_{ListenerStatus::Unsupported};
    std::string error_;
};

} // namespace puffy::hotkeys
