#pragma once

#include "core/audio/audio_ports.hpp"

namespace puffy::platform::windows {

class WasapiCapture final : public audio::IAudioCapture {
public:
    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string&, audio::AudioFormat, Callback) override;
    void close() noexcept override;
};

class WasapiOutput final : public audio::IAudioOutput {
public:
    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string&, audio::AudioFormat) override;
    bool start() override;
    void stop() noexcept override;
    bool write(std::span<const float>, std::size_t) noexcept override;
};

class WindowsVirtualMicrophone final : public audio::IVirtualMicrophone {
public:
    bool initialize() override;
    bool start() override;
    void stop() noexcept override;
    bool pushAudio(std::span<const float>, std::size_t, int, int) noexcept override;
};

} // namespace puffy::platform::windows
