#include "platform/windows/windows_global_keyboard_listener.hpp"

#include "core/hotkeys/hotkey_manager.hpp"

#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cctype>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace puffy::platform::windows {

struct WindowsGlobalKeyboardListener::State {
    std::thread thread;
    std::atomic<bool> running{false};
    DWORD threadId{0};
    HHOOK hook{nullptr};
    Callback callback;
    std::array<bool, 256> down{};
    std::mutex readyMutex;
    std::condition_variable readyCondition;
    bool ready{false};
    bool succeeded{false};
};

namespace {
std::atomic<WindowsGlobalKeyboardListener::State*> activeState{nullptr};

bool isDown(const WindowsGlobalKeyboardListener::State& state, int left, int right) {
    return state.down[static_cast<std::size_t>(left)] || state.down[static_cast<std::size_t>(right)];
}

LRESULT CALLBACK keyboardProcedure(int code, WPARAM message, LPARAM value) {
    auto* state = activeState.load(std::memory_order_acquire);
    if (code == HC_ACTION && state != nullptr && state->callback) {
        const auto* data = reinterpret_cast<const KBDLLHOOKSTRUCT*>(value);
        const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        const bool released = message == WM_KEYUP || message == WM_SYSKEYUP;
        if ((pressed || released) && data->vkCode < state->down.size()) {
            const auto key = static_cast<std::size_t>(data->vkCode);
            const bool repeat = pressed && state->down[key];
            state->down[key] = pressed;
            state->callback({static_cast<int>(data->vkCode), pressed, repeat,
                             isDown(*state, VK_LCONTROL, VK_RCONTROL),
                             isDown(*state, VK_LSHIFT, VK_RSHIFT),
                             isDown(*state, VK_LMENU, VK_RMENU),
                             isDown(*state, VK_LWIN, VK_RWIN)});
        }
    }
    return CallNextHookEx(nullptr, code, message, value);
}

const std::unordered_map<std::string, int> keyNames{
    {"CTRL", VK_CONTROL}, {"SHIFT", VK_SHIFT}, {"ALT", VK_MENU}, {"SUPER", VK_LWIN},
    {"WIN", VK_LWIN}, {"SPACE", VK_SPACE}, {"ENTER", VK_RETURN}, {"RETURN", VK_RETURN},
    {"ESC", VK_ESCAPE}, {"ESCAPE", VK_ESCAPE}, {"TAB", VK_TAB}, {"BACKSPACE", VK_BACK},
    {"DELETE", VK_DELETE}, {"INSERT", VK_INSERT}, {"HOME", VK_HOME}, {"END", VK_END},
    {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT}, {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT},
    {"UP", VK_UP}, {"DOWN", VK_DOWN}, {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3},
    {"F4", VK_F4}, {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
    {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},
    {"F13", VK_F13}, {"F14", VK_F14}, {"F15", VK_F15}, {"F16", VK_F16},
    {"F17", VK_F17}, {"F18", VK_F18}, {"F19", VK_F19}, {"F20", VK_F20},
    {"F21", VK_F21}, {"F22", VK_F22}, {"F23", VK_F23}, {"F24", VK_F24},
};
} // namespace

WindowsGlobalKeyboardListener::WindowsGlobalKeyboardListener() = default;
WindowsGlobalKeyboardListener::~WindowsGlobalKeyboardListener() { stop(); }

hotkeys::ListenerStatus WindowsGlobalKeyboardListener::start(Callback callback) {
    if (state_ || !callback) {
        error_ = "Windows keyboard listener is already running or callback is empty";
        return hotkeys::ListenerStatus::Failed;
    }
    auto state = std::make_unique<State>();
    state->callback = std::move(callback);
    state->running = true;
    state->thread = std::thread([raw = state.get()] {
        raw->threadId = GetCurrentThreadId();
        MSG message{};
        PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        WindowsGlobalKeyboardListener::State* expected = nullptr;
        const bool exclusive = activeState.compare_exchange_strong(expected, raw, std::memory_order_acq_rel);
        if (exclusive) raw->hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProcedure, GetModuleHandleW(nullptr), 0);
        {
            std::lock_guard lock(raw->readyMutex);
            raw->succeeded = raw->hook != nullptr;
            raw->ready = true;
        }
        raw->readyCondition.notify_one();
        if (raw->hook != nullptr) {
            while (raw->running && GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            UnhookWindowsHookEx(raw->hook);
            raw->hook = nullptr;
        }
        activeState.compare_exchange_strong(raw, nullptr, std::memory_order_acq_rel);
    });
    {
        std::unique_lock lock(state->readyMutex);
        state->readyCondition.wait(lock, [&state] { return state->ready; });
    }
    if (!state->succeeded) {
        state->running = false;
        if (state->threadId) PostThreadMessageW(state->threadId, WM_QUIT, 0, 0);
        if (state->thread.joinable()) state->thread.join();
        error_ = "Unable to install WH_KEYBOARD_LL hook";
        return hotkeys::ListenerStatus::Failed;
    }
    state_ = std::move(state);
    error_.clear();
    return hotkeys::ListenerStatus::Ready;
}

void WindowsGlobalKeyboardListener::stop() noexcept {
    if (!state_) return;
    state_->running = false;
    if (state_->threadId) PostThreadMessageW(state_->threadId, WM_QUIT, 0, 0);
    if (state_->thread.joinable()) state_->thread.join();
    state_.reset();
}

std::optional<int> WindowsGlobalKeyboardListener::resolveKeyCode(std::string_view name) const {
    std::string key(name);
    for (auto& value : key) value = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
    if (key.size() == 1) {
        const auto translated = VkKeyScanW(static_cast<wchar_t>(key.front()));
        if (translated != -1) return static_cast<int>(LOBYTE(translated));
    }
    const auto found = keyNames.find(key);
    return found == keyNames.end() ? std::nullopt : std::optional<int>(found->second);
}

} // namespace puffy::platform::windows
