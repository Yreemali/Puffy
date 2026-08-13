#pragma once

#include "core/audio/audio_ports.hpp"

#include <memory>
#include <string>

namespace puffy::platform::windows {

class WasapiCapture final : public audio::IAudioCapture {
public:
    struct State;
    WasapiCapture();
    ~WasapiCapture() override;
    WasapiCapture(const WasapiCapture&) = delete;
    WasapiCapture& operator=(const WasapiCapture&) = delete;

    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string& deviceId, audio::AudioFormat format, Callback callback) override;
    void close() noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override;

private:
    std::unique_ptr<State> state_;
};

class WasapiOutput final : public audio::IAudioOutput {
public:
    struct State;
    explicit WasapiOutput(std::size_t ringBufferSamples = 16U * 1024U);
    ~WasapiOutput() override;
    WasapiOutput(const WasapiOutput&) = delete;
    WasapiOutput& operator=(const WasapiOutput&) = delete;

    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string& deviceId, audio::AudioFormat format) override;
    bool start() override;
    void stop() noexcept override;
    bool write(std::span<const float> samples, std::size_t frames) noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override;

private:
    std::unique_ptr<State> state_;
};

/**
 * Sends audio to the render endpoint exposed by Puffy's separately installed
 * virtual-audio driver. The endpoint ID is opaque and comes from MMDevice.
 */
class WindowsVirtualMicrophone final : public audio::IVirtualMicrophone {
public:
    explicit WindowsVirtualMicrophone(std::string transportEndpointId = {});
    ~WindowsVirtualMicrophone() override;
    void setTransportEndpointId(std::string endpointId);
    bool initialize() override;
    bool start() override;
    void stop() noexcept override;
    bool pushAudio(std::span<const float> samples, std::size_t frames,
                   int channels, int sampleRate) noexcept override;

private:
    std::string endpointId_;
    std::unique_ptr<WasapiOutput> output_;
    audio::AudioFormat format_{};
    bool initialized_{false};
};

} // namespace puffy::platform::windows
