#include "core/hotkeys/hotkey_router.hpp"

#include <charconv>
#include <optional>

namespace {
std::optional<int> parseKey(std::string_view value) {
    int key = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), key);
    if (result.ec == std::errc{} && result.ptr == value.data() + value.size()) return key;
    return std::nullopt;
}
} // namespace

namespace puffy::hotkeys {

HotkeyRouter::HotkeyRouter(IGlobalKeyboardListener& listener, soundboard::SoundboardService& service)
    : listener_(listener), service_(service) {}

HotkeyRouter::~HotkeyRouter() { stop(); }

bool HotkeyRouter::start() {
    const auto callback = [this](const KeyEvent& event) {
        manager_.handle(event);
        if (!event.pressed) return;
        if (fullKeyboard_.ignoresEvent(event.ctrl, event.shift, event.alt, event.super, event.keyCode)) return;
        const auto soundId = fullKeyboard_.onKeyPress(event.keyCode, event.repeat);
        if (soundId >= 0) service_.trigger(soundId, true, event.repeat);
    };
    status_ = listener_.start(callback);
    if (status_ != ListenerStatus::Ready) {
        error_ = listener_.lastError();
        return false;
    }
    error_.clear();
    return true;
}

void HotkeyRouter::stop() noexcept { listener_.stop(); }

bool HotkeyRouter::bindSound(Hotkey hotkey, std::int64_t soundId) {
    return manager_.bindEvent(hotkey, [this, soundId](const KeyEvent& event) {
        service_.trigger(soundId, event.pressed, event.repeat);
    });
}

bool HotkeyRouter::bindSoundText(std::string_view text, std::int64_t soundId) {
    return bindActionText(text, [this, soundId](const KeyEvent& event) {
        service_.trigger(soundId, event.pressed, event.repeat);
    });
}

bool HotkeyRouter::bindActionText(std::string_view text, HotkeyManager::EventAction action) {
    Hotkey hotkey;
    std::size_t begin = 0;
    while (begin < text.size()) {
        const auto end = text.find('+', begin);
        const auto token = text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin);
        if (token == "CTRL") hotkey.ctrl = true;
        else if (token == "SHIFT") hotkey.shift = true;
        else if (token == "ALT") hotkey.alt = true;
        else if (token == "SUPER") hotkey.super = true;
        else if (const auto key = parseKey(token)) hotkey.keyCode = *key;
        else if (const auto key = listener_.resolveKeyCode(token)) hotkey.keyCode = *key;
        else return false;
        if (end == std::string_view::npos) break;
        begin = end + 1;
    }
    return hotkey.keyCode != 0 && manager_.bindEvent(hotkey, std::move(action));
}

void HotkeyRouter::clearBindings() { manager_.clear(); }

} // namespace puffy::hotkeys
