#include "core/hotkeys/hotkey_manager.hpp"
#include "core/soundboard/full_keyboard_mode.hpp"

#include <iostream>

int main() {
    puffy::soundboard::FullKeyboardModeController keyboard;
    keyboard.setEnabled(true);
    keyboard.setMode(puffy::soundboard::FullKeyboardMode::Sequential);
    keyboard.setPlaylist({1, 2, 3});
    std::cout << "puffy core demo: " << keyboard.onKeyPress(65) << "\n";
    return 0;
}
