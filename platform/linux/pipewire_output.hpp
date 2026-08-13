#pragma once

#include "core/audio/audio_ports.hpp"

#include <memory>

namespace puffy::platform::linux {

class PipeWireOutput final : public audio::IAudioOutput {
public:
    struct State;
    // Four small realtime periods. Keeping this bounded prevents stale audio
    // from turning a temporary scheduling hiccup into seconds of latency.
    explicit PipeWireOutput(std::size_t ringBufferBytes = 16U * 1024U);
    ~PipeWireOutput() override;

    PipeWireOutput(const PipeWireOutput&) = delete;
    PipeWireOutput& operator=(const PipeWireOutput&) = delete;

    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string& deviceId, audio::AudioFormat format) override;
    bool start() override;
    void stop() noexcept override;
    bool write(std::span<const float> interleavedSamples, std::size_t frames) noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override;

private:
    std::unique_ptr<State> state_;
};

} // namespace puffy::platform::linux
