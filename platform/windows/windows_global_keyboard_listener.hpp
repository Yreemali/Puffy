#pragma once

#include "core/hotkeys/global_keyboard_listener.hpp"

#include <memory>
#include <string>

namespace puffy::platform::windows {

class WindowsGlobalKeyboardListener final : public hotkeys::IGlobalKeyboardListener {
public:
    struct State;
    WindowsGlobalKeyboardListener();
    ~WindowsGlobalKeyboardListener() override;
    WindowsGlobalKeyboardListener(const WindowsGlobalKeyboardListener&) = delete;
    WindowsGlobalKeyboardListener& operator=(const WindowsGlobalKeyboardListener&) = delete;

    hotkeys::ListenerStatus start(Callback callback) override;
    void stop() noexcept override;
    [[nodiscard]] std::optional<int> resolveKeyCode(std::string_view name) const override;
    [[nodiscard]] const std::string& lastError() const noexcept override { return error_; }

private:
    std::unique_ptr<State> state_;
    std::string error_;
};

} // namespace puffy::platform::windows
