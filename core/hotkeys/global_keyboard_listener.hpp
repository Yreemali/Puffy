#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace puffy::hotkeys {

struct KeyEvent;

enum class ListenerStatus {
    Ready,
    Unsupported,
    PermissionDenied,
    Failed,
};

class IGlobalKeyboardListener {
public:
    using Callback = std::function<void(const KeyEvent&)>;
    virtual ~IGlobalKeyboardListener() = default;
    virtual ListenerStatus start(Callback callback) = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual std::optional<int> resolveKeyCode(std::string_view) const { return std::nullopt; }
    [[nodiscard]] virtual const std::string& lastError() const noexcept = 0;
};

} // namespace puffy::hotkeys
