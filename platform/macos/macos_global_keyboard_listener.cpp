#include "platform/macos/macos_global_keyboard_listener.hpp"

#include "core/hotkeys/hotkey_manager.hpp"

#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cctype>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace puffy::platform::macos {

struct MacOSGlobalKeyboardListener::State {
    std::thread thread;
    std::atomic<bool> running{false};
    Callback callback;
    CFMachPortRef tap{nullptr};
    CFRunLoopSourceRef source{nullptr};
    CFRunLoopRef runLoop{nullptr};
    std::array<bool, 256> down{};
    std::mutex readyMutex;
    std::condition_variable readyCondition;
    bool ready{false};
    bool succeeded{false};
};

namespace {
bool modifierPressed(CGKeyCode code, CGEventFlags flags, bool fallback) {
    switch (code) {
    case kVK_Shift: case kVK_RightShift: return (flags & kCGEventFlagMaskShift) != 0;
    case kVK_Control: case kVK_RightControl: return (flags & kCGEventFlagMaskControl) != 0;
    case kVK_Option: case kVK_RightOption: return (flags & kCGEventFlagMaskAlternate) != 0;
    case kVK_Command: case kVK_RightCommand: return (flags & kCGEventFlagMaskCommand) != 0;
    default: return fallback;
    }
}

CGEventRef eventCallback(CGEventTapProxy, CGEventType type, CGEventRef event, void* userData) {
    auto& state = *static_cast<MacOSGlobalKeyboardListener::State*>(userData);
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (state.tap) CGEventTapEnable(state.tap, true);
        return event;
    }
    if (!state.callback || (type != kCGEventKeyDown && type != kCGEventKeyUp && type != kCGEventFlagsChanged)) return event;
    const auto code = static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
    if (code >= state.down.size()) return event;
    const auto flags = CGEventGetFlags(event);
    const bool pressed = type == kCGEventFlagsChanged
        ? modifierPressed(code, flags, !state.down[code]) : type == kCGEventKeyDown;
    const bool repeat = pressed && (CGEventGetIntegerValueField(event, kCGKeyboardEventAutorepeat) != 0 || state.down[code]);
    state.down[code] = pressed;
    state.callback({static_cast<int>(code), pressed, repeat,
                    (flags & kCGEventFlagMaskControl) != 0,
                    (flags & kCGEventFlagMaskShift) != 0,
                    (flags & kCGEventFlagMaskAlternate) != 0,
                    (flags & kCGEventFlagMaskCommand) != 0});
    return event; // Listen-only tap: the original event always continues to the system.
}

const std::unordered_map<std::string, int> keyNames{
    {"CTRL", kVK_Control}, {"SHIFT", kVK_Shift}, {"ALT", kVK_Option},
    {"OPTION", kVK_Option}, {"SUPER", kVK_Command}, {"CMD", kVK_Command},
    {"COMMAND", kVK_Command}, {"SPACE", kVK_Space}, {"ENTER", kVK_Return},
    {"RETURN", kVK_Return}, {"ESC", kVK_Escape}, {"ESCAPE", kVK_Escape},
    {"TAB", kVK_Tab}, {"BACKSPACE", kVK_Delete}, {"DELETE", kVK_ForwardDelete},
    {"HOME", kVK_Home}, {"END", kVK_End}, {"PAGEUP", kVK_PageUp},
    {"PAGEDOWN", kVK_PageDown}, {"LEFT", kVK_LeftArrow}, {"RIGHT", kVK_RightArrow},
    {"UP", kVK_UpArrow}, {"DOWN", kVK_DownArrow}, {"F1", kVK_F1}, {"F2", kVK_F2},
    {"F3", kVK_F3}, {"F4", kVK_F4}, {"F5", kVK_F5}, {"F6", kVK_F6},
    {"F7", kVK_F7}, {"F8", kVK_F8}, {"F9", kVK_F9}, {"F10", kVK_F10},
    {"F11", kVK_F11}, {"F12", kVK_F12}, {"F13", kVK_F13}, {"F14", kVK_F14},
    {"F15", kVK_F15}, {"F16", kVK_F16}, {"F17", kVK_F17}, {"F18", kVK_F18},
    {"F19", kVK_F19}, {"F20", kVK_F20},
};

const std::unordered_map<char, int> characterKeys{
    {'A', kVK_ANSI_A}, {'B', kVK_ANSI_B}, {'C', kVK_ANSI_C}, {'D', kVK_ANSI_D},
    {'E', kVK_ANSI_E}, {'F', kVK_ANSI_F}, {'G', kVK_ANSI_G}, {'H', kVK_ANSI_H},
    {'I', kVK_ANSI_I}, {'J', kVK_ANSI_J}, {'K', kVK_ANSI_K}, {'L', kVK_ANSI_L},
    {'M', kVK_ANSI_M}, {'N', kVK_ANSI_N}, {'O', kVK_ANSI_O}, {'P', kVK_ANSI_P},
    {'Q', kVK_ANSI_Q}, {'R', kVK_ANSI_R}, {'S', kVK_ANSI_S}, {'T', kVK_ANSI_T},
    {'U', kVK_ANSI_U}, {'V', kVK_ANSI_V}, {'W', kVK_ANSI_W}, {'X', kVK_ANSI_X},
    {'Y', kVK_ANSI_Y}, {'Z', kVK_ANSI_Z}, {'0', kVK_ANSI_0}, {'1', kVK_ANSI_1},
    {'2', kVK_ANSI_2}, {'3', kVK_ANSI_3}, {'4', kVK_ANSI_4}, {'5', kVK_ANSI_5},
    {'6', kVK_ANSI_6}, {'7', kVK_ANSI_7}, {'8', kVK_ANSI_8}, {'9', kVK_ANSI_9},
};
} // namespace

MacOSGlobalKeyboardListener::MacOSGlobalKeyboardListener() = default;
MacOSGlobalKeyboardListener::~MacOSGlobalKeyboardListener() { stop(); }

hotkeys::ListenerStatus MacOSGlobalKeyboardListener::start(Callback callback) {
    if (state_ || !callback) {
        error_ = "macOS keyboard listener is already running or callback is empty";
        return hotkeys::ListenerStatus::Failed;
    }
    if (!CGPreflightListenEventAccess() && !CGRequestListenEventAccess()) {
        error_ = "Input Monitoring permission is required. Enable Puffy in Privacy & Security > Input Monitoring.";
        return hotkeys::ListenerStatus::PermissionDenied;
    }
    auto state = std::make_unique<State>();
    state->callback = std::move(callback);
    state->running = true;
    state->thread = std::thread([raw = state.get()] {
        const auto mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                          CGEventMaskBit(kCGEventFlagsChanged);
        raw->tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                    kCGEventTapOptionListenOnly, mask, eventCallback, raw);
        if (raw->tap != nullptr) {
            raw->source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, raw->tap, 0);
            raw->runLoop = CFRunLoopGetCurrent();
            CFRetain(raw->runLoop);
            if (raw->source != nullptr) {
                CFRunLoopAddSource(raw->runLoop, raw->source, kCFRunLoopCommonModes);
                CGEventTapEnable(raw->tap, true);
            }
        }
        {
            std::lock_guard lock(raw->readyMutex);
            raw->succeeded = raw->tap != nullptr && raw->source != nullptr;
            raw->ready = true;
        }
        raw->readyCondition.notify_one();
        if (raw->succeeded) CFRunLoopRun();
        if (raw->source != nullptr) {
            CFRunLoopRemoveSource(raw->runLoop, raw->source, kCFRunLoopCommonModes);
            CFRelease(raw->source);
        }
        if (raw->tap != nullptr) CFRelease(raw->tap);
        if (raw->runLoop != nullptr) CFRelease(raw->runLoop);
        raw->source = nullptr; raw->tap = nullptr; raw->runLoop = nullptr;
    });
    {
        std::unique_lock lock(state->readyMutex);
        state->readyCondition.wait(lock, [&state] { return state->ready; });
    }
    if (!state->succeeded) {
        state->running = false;
        if (state->thread.joinable()) state->thread.join();
        error_ = "Unable to create a passive macOS keyboard event tap";
        return hotkeys::ListenerStatus::Failed;
    }
    state_ = std::move(state);
    error_.clear();
    return hotkeys::ListenerStatus::Ready;
}

void MacOSGlobalKeyboardListener::stop() noexcept {
    if (!state_) return;
    state_->running = false;
    if (state_->runLoop != nullptr) CFRunLoopStop(state_->runLoop);
    if (state_->thread.joinable()) state_->thread.join();
    state_.reset();
}

std::optional<int> MacOSGlobalKeyboardListener::resolveKeyCode(std::string_view name) const {
    std::string key(name);
    for (auto& value : key) value = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
    if (key.size() == 1) {
        const auto found = characterKeys.find(key.front());
        if (found != characterKeys.end()) return found->second;
    }
    const auto found = keyNames.find(key);
    return found == keyNames.end() ? std::nullopt : std::optional<int>(found->second);
}

} // namespace puffy::platform::macos
