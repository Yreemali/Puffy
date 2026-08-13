#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace puffy::hotkeys {

struct KeyEvent {
    int keyCode{};
    bool pressed{false};
    bool repeat{false};
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool super{false};
};

struct Hotkey {
    int keyCode{};
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool super{false};

    bool operator==(const Hotkey&) const = default;
};

struct HotkeyHash {
    std::size_t operator()(const Hotkey& key) const noexcept;
};

class HotkeyManager final {
public:
    using Action = std::function<void()>;
    using EventAction = std::function<void(const KeyEvent&)>;

    bool bind(Hotkey hotkey, Action action);
    bool bindEvent(Hotkey hotkey, EventAction action);
    void unbind(Hotkey hotkey);
    void clear();
    void handle(const KeyEvent& event) const;

private:
    std::unordered_map<Hotkey, Action, HotkeyHash> bindings_;
    std::unordered_map<Hotkey, EventAction, HotkeyHash> eventBindings_;
};

} // namespace puffy::hotkeys
