#pragma once

#include "core/hotkeys/global_keyboard_listener.hpp"
#include <memory>

namespace puffy::platform::linux {

class EvdevGlobalKeyboardListener final : public hotkeys::IGlobalKeyboardListener {
public:
    struct State;
    EvdevGlobalKeyboardListener();
    ~EvdevGlobalKeyboardListener() override;
    hotkeys::ListenerStatus start(Callback callback) override;
    void stop() noexcept override;
    [[nodiscard]] std::optional<int> resolveKeyCode(std::string_view name) const override;
    [[nodiscard]] const std::string& lastError() const noexcept override { return error_; }
private:
    std::unique_ptr<State> state_;
    std::string error_;
};
}
