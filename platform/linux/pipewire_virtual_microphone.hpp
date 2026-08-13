#pragma once

#include "core/audio/audio_ports.hpp"

#include <memory>

namespace puffy::platform::linux {

class PipeWireVirtualMicrophone final : public audio::IVirtualMicrophone {
public:
    struct State;
    explicit PipeWireVirtualMicrophone(std::size_t ringBufferBytes = 48000U * sizeof(float) * 2U * 2U);
    ~PipeWireVirtualMicrophone() override;

    PipeWireVirtualMicrophone(const PipeWireVirtualMicrophone&) = delete;
    PipeWireVirtualMicrophone& operator=(const PipeWireVirtualMicrophone&) = delete;

    bool initialize() override;
    bool start() override;
    void stop() noexcept override;
    bool pushAudio(std::span<const float> samples, std::size_t frames,
                   int channels, int sampleRate) noexcept override;

private:
    std::unique_ptr<State> state_;
};

} // namespace puffy::platform::linux
