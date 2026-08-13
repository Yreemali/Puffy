#pragma once

#include "core/audio/audio_ports.hpp"

#include <memory>

namespace puffy::platform::linux {

class PipeWireCapture final : public audio::IAudioCapture {
public:
    struct State;

    PipeWireCapture();
    ~PipeWireCapture() override;

    PipeWireCapture(const PipeWireCapture&) = delete;
    PipeWireCapture& operator=(const PipeWireCapture&) = delete;

    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string& deviceId, audio::AudioFormat format, Callback callback) override;
    void close() noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override;

private:
    std::unique_ptr<State> state_;
};

} // namespace puffy::platform::linux
