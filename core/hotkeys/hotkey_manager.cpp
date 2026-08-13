#include "core/hotkeys/hotkey_manager.hpp"

namespace puffy::hotkeys {

std::size_t HotkeyHash::operator()(const Hotkey& key) const noexcept {
    auto value = static_cast<std::size_t>(key.keyCode);
    value = value * 31U + key.ctrl;
    value = value * 31U + key.shift;
    value = value * 31U + key.alt;
    return value * 31U + key.super;
}

bool HotkeyManager::bind(Hotkey hotkey, Action action) {
    if (!action) return false;
    bindings_[hotkey] = std::move(action);
    return true;
}

void HotkeyManager::unbind(Hotkey hotkey) { bindings_.erase(hotkey); }
void HotkeyManager::clear() { bindings_.clear(); }

void HotkeyManager::handle(const KeyEvent& event) const {
    if (!event.pressed) return;
    const Hotkey key{event.keyCode, event.ctrl, event.shift, event.alt, event.super};
    if (const auto it = bindings_.find(key); it != bindings_.end()) it->second();
}

} // namespace puffy::hotkeys
