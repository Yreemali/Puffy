#include "platform/linux/evdev_global_keyboard_listener.hpp"
#include "core/hotkeys/hotkey_manager.hpp"

#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>

namespace puffy::platform::linux {
struct EvdevGlobalKeyboardListener::State {
    std::vector<int> fds;
    Callback callback;
    std::thread thread;
    std::atomic<bool> running{false};
    bool ctrl{false}, shift{false}, alt{false}, super{false};
};

namespace {
bool isKeyboard(int fd) {
    unsigned long bits[(KEY_MAX + 8 * sizeof(unsigned long)) / (8 * sizeof(unsigned long))]{};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits) < 0) return false;
    auto has = [&](int key) { return (bits[key / (8 * sizeof(unsigned long))] >> (key % (8 * sizeof(unsigned long)))) & 1UL; };
    return has(KEY_A) && has(KEY_Z) && has(KEY_SPACE);
}
void setModifier(EvdevGlobalKeyboardListener::State& s, int code, bool down) {
    if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) s.ctrl = down;
    if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) s.shift = down;
    if (code == KEY_LEFTALT || code == KEY_RIGHTALT) s.alt = down;
    if (code == KEY_LEFTMETA || code == KEY_RIGHTMETA) s.super = down;
}
const std::unordered_map<std::string, int> names{{"CTRL",KEY_LEFTCTRL},{"SHIFT",KEY_LEFTSHIFT},{"ALT",KEY_LEFTALT},{"SUPER",KEY_LEFTMETA},{"SPACE",KEY_SPACE},{"ENTER",KEY_ENTER},{"ESC",KEY_ESC},{"TAB",KEY_TAB},{"BACKSPACE",KEY_BACKSPACE},{"F1",KEY_F1},{"F2",KEY_F2},{"F3",KEY_F3},{"F4",KEY_F4},{"F5",KEY_F5},{"F6",KEY_F6},{"F7",KEY_F7},{"F8",KEY_F8},{"F9",KEY_F9},{"F10",KEY_F10},{"F11",KEY_F11},{"F12",KEY_F12}};
}

EvdevGlobalKeyboardListener::EvdevGlobalKeyboardListener() = default;
EvdevGlobalKeyboardListener::~EvdevGlobalKeyboardListener() { stop(); }

hotkeys::ListenerStatus EvdevGlobalKeyboardListener::start(Callback callback) {
    if (state_ || !callback) { error_ = "Evdev listener is already running"; return hotkeys::ListenerStatus::Failed; }
    auto state = std::make_unique<State>(); state->callback = std::move(callback);
    std::error_code directoryError;
    for (const auto& entry : std::filesystem::directory_iterator("/dev/input", directoryError)) {
        if (entry.path().filename().string().rfind("event", 0) != 0) continue;
        const auto fd = ::open(entry.path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0 && isKeyboard(fd)) state->fds.push_back(fd); else if (fd >= 0) ::close(fd);
    }
    if (state->fds.empty()) { error_ = "Wayland keyboard access is unavailable. Add the user to the input group or grant access to /dev/input/event*."; return hotkeys::ListenerStatus::PermissionDenied; }
    state->running = true;
    state->thread = std::thread([raw = state.get()] {
        std::vector<pollfd> pollers; for (const auto fd : raw->fds) pollers.push_back({fd, POLLIN, 0});
        while (raw->running) {
            if (::poll(pollers.data(), pollers.size(), 100) <= 0) continue;
            for (const auto& p : pollers) if (p.revents & POLLIN) { input_event event{}; while (::read(p.fd, &event, sizeof(event)) == sizeof(event)) if (event.type == EV_KEY && event.value <= 2) { const bool down = event.value != 0; setModifier(*raw, event.code, down); if (raw->callback) raw->callback({event.code, down, event.value == 2, raw->ctrl, raw->shift, raw->alt, raw->super}); } }
        }
    });
    state_ = std::move(state); return hotkeys::ListenerStatus::Ready;
}

void EvdevGlobalKeyboardListener::stop() noexcept { if (!state_) return; state_->running = false; if (state_->thread.joinable()) state_->thread.join(); for (auto fd : state_->fds) ::close(fd); state_.reset(); }
std::optional<int> EvdevGlobalKeyboardListener::resolveKeyCode(std::string_view name) const {
    if (name.size() == 1) {
        const auto c = static_cast<char>(std::toupper(static_cast<unsigned char>(name.front())));
        if (c >= 'A' && c <= 'Z') return KEY_A + (c - 'A');
        if (c >= '1' && c <= '9') return KEY_1 + (c - '1');
        if (c == '0') return KEY_0;
    }
    const auto it = names.find(std::string(name));
    return it == names.end() ? std::nullopt : std::optional<int>(it->second);
}
}
