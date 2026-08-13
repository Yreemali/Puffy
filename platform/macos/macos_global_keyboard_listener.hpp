#pragma once

#include "core/hotkeys/global_keyboard_listener.hpp"

#include <memory>
#include <string>

namespace puffy::platform::macos {

class MacOSGlobalKeyboardListener final : public hotkeys::IGlobalKeyboardListener {
public:
    struct State;
    MacOSGlobalKeyboardListener();
    ~MacOSGlobalKeyboardListener() override;
    MacOSGlobalKeyboardListener(const MacOSGlobalKeyboardListener&) = delete;
    MacOSGlobalKeyboardListener& operator=(const MacOSGlobalKeyboardListener&) = delete;

    hotkeys::ListenerStatus start(Callback callback) override;
    void stop() noexcept override;
    [[nodiscard]] std::optional<int> resolveKeyCode(std::string_view name) const override;
    [[nodiscard]] const std::string& lastError() const noexcept override { return error_; }

private:
    std::unique_ptr<State> state_;
    std::string error_;
};

} // namespace puffy::platform::macos
