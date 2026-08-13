#include "platform/linux/x11_global_keyboard_listener.hpp"

#include "core/hotkeys/hotkey_manager.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/record.h>
#include <X11/keysym.h>

#include <atomic>
#include <cctype>
#include <string>
#include <thread>

namespace puffy::platform::linux {

struct X11GlobalKeyboardListener::State {
    Display* control{nullptr};
    Display* data{nullptr};
    XRecordContext context{0};
    std::thread thread;
    std::atomic<bool> running{false};
    Callback callback;
};

namespace {

int ignoreX11Error(Display*, XErrorEvent*) {
    // XRecord can be unavailable or broken under some XWayland setups.
    // A global keyboard listener must never be allowed to terminate the UI.
    return 0;
}

void recordCallback(XPointer userData, XRecordInterceptData* intercepted) {
    auto& state = *reinterpret_cast<X11GlobalKeyboardListener::State*>(userData);
    if (intercepted->category == XRecordFromServer && intercepted->data_len >= 2) {
        const auto* event = reinterpret_cast<const unsigned char*>(intercepted->data);
        const auto type = event[0] & 0x7FU;
        if ((type == KeyPress || type == KeyRelease) && state.callback) {
            const auto x11State = intercepted->data_len >= 9
                ? static_cast<unsigned short>(event[32] | (event[33] << 8U)) : 0U;
            const hotkeys::KeyEvent key{
                static_cast<int>(event[1]), type == KeyPress, false,
                (x11State & ControlMask) != 0, (x11State & ShiftMask) != 0,
                (x11State & Mod1Mask) != 0, (x11State & Mod4Mask) != 0};
            state.callback(key);
        }
    }
    XRecordFreeData(intercepted);
}

} // namespace

X11GlobalKeyboardListener::X11GlobalKeyboardListener() = default;
X11GlobalKeyboardListener::~X11GlobalKeyboardListener() { stop(); }

hotkeys::ListenerStatus X11GlobalKeyboardListener::start(Callback callback) {
    if (state_ != nullptr || !callback) { error_ = "Listener is already running or callback is empty"; return hotkeys::ListenerStatus::Failed; }
    XInitThreads();
    auto state = std::make_unique<State>();
    state->control = XOpenDisplay(nullptr);
    state->data = XOpenDisplay(nullptr);
    if (state->control == nullptr || state->data == nullptr) {
        if (state->control) XCloseDisplay(state->control);
        if (state->data) XCloseDisplay(state->data);
        error_ = "X11 display is unavailable; Wayland requires a compositor-specific backend";
        return hotkeys::ListenerStatus::Unsupported;
    }
    int major = 0, minor = 0;
    if (!XRecordQueryVersion(state->control, &major, &minor)) {
        error_ = "XRecord extension is unavailable";
        XCloseDisplay(state->control); XCloseDisplay(state->data);
        return hotkeys::ListenerStatus::Unsupported;
    }
    XRecordRange* range = XRecordAllocRange();
    if (range == nullptr) { error_ = "Cannot allocate XRecord range"; return hotkeys::ListenerStatus::Failed; }
    range->device_events.first = KeyPress;
    range->device_events.last = KeyRelease;
    XRecordRange* ranges[] = {range};
    state->context = XRecordCreateContext(state->control, 0, nullptr, 0, ranges, 1);
    XFree(range);
    // XRecordCreateContext is asynchronous.  Flush and wait for the request
    // before using the context through the second display connection.
    XSync(state->control, False);
    if (state->context == 0) {
        error_ = "Cannot create XRecord context";
        XCloseDisplay(state->control); XCloseDisplay(state->data);
        return hotkeys::ListenerStatus::Failed;
    }
    state->callback = std::move(callback);
    state->running = true;
    // Some XWayland servers report XRecord failures asynchronously.  Xlib's
    // default handler calls exit(), which is unacceptable for an optional
    // hotkey backend.  Keep the listener best-effort instead.
    XSetErrorHandler(ignoreX11Error);
    state->thread = std::thread([raw = state.get()] {
        XRecordEnableContext(raw->data, raw->context, recordCallback, reinterpret_cast<XPointer>(raw));
        raw->running = false;
    });
    state_ = std::move(state);
    return hotkeys::ListenerStatus::Ready;
}

void X11GlobalKeyboardListener::stop() noexcept {
    if (state_ == nullptr) return;
    state_->running = false;
    XRecordDisableContext(state_->control, state_->context);
    XFlush(state_->control);
    if (state_->thread.joinable()) state_->thread.join();
    XRecordFreeContext(state_->control, state_->context);
    XCloseDisplay(state_->control);
    XCloseDisplay(state_->data);
    state_.reset();
}

std::optional<int> X11GlobalKeyboardListener::resolveKeyCode(std::string_view name) const {
    if (state_ == nullptr || state_->control == nullptr) return std::nullopt;
    std::string key(name);
    if (key.size() == 1) key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
    const auto symbol = XStringToKeysym(key.c_str());
    if (symbol == NoSymbol) return std::nullopt;
    const auto code = XKeysymToKeycode(state_->control, symbol);
    return code == 0 ? std::nullopt : std::optional<int>(static_cast<int>(code));
}

} // namespace puffy::platform::linux
