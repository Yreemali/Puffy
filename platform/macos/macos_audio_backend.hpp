#pragma once

#include "core/audio/audio_ports.hpp"

#include <memory>
#include <string>

namespace puffy::platform::macos {

class CoreAudioCapture final : public audio::IAudioCapture {
public:
    struct State;
    CoreAudioCapture();
    ~CoreAudioCapture() override;
    CoreAudioCapture(const CoreAudioCapture&) = delete;
    CoreAudioCapture& operator=(const CoreAudioCapture&) = delete;

    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string& deviceId, audio::AudioFormat format, Callback callback) override;
    void close() noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override;

private:
    std::unique_ptr<State> state_;
};

class CoreAudioOutput final : public audio::IAudioOutput {
public:
    struct State;
    explicit CoreAudioOutput(std::size_t ringBufferSamples = 16U * 1024U);
    ~CoreAudioOutput() override;
    CoreAudioOutput(const CoreAudioOutput&) = delete;
    CoreAudioOutput& operator=(const CoreAudioOutput&) = delete;

    [[nodiscard]] std::vector<audio::AudioDeviceInfo> devices() const override;
    bool open(const std::string& deviceId, audio::AudioFormat format) override;
    bool start() override;
    void stop() noexcept override;
    bool write(std::span<const float> samples, std::size_t frames) noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override;

private:
    std::unique_ptr<State> state_;
};

/** Sender for the transport output published by Puffy's separate HAL plug-in. */
class MacOSVirtualMicrophone final : public audio::IVirtualMicrophone {
public:
    explicit MacOSVirtualMicrophone(std::string transportDeviceUid = {});
    ~MacOSVirtualMicrophone() override;
    void setTransportDeviceUid(std::string deviceUid);
    bool initialize() override;
    bool start() override;
    void stop() noexcept override;
    bool pushAudio(std::span<const float> samples, std::size_t frames,
                   int channels, int sampleRate) noexcept override;

private:
    std::string deviceUid_;
    std::unique_ptr<CoreAudioOutput> output_;
    audio::AudioFormat format_{};
    bool initialized_{false};
};

} // namespace puffy::platform::macos
