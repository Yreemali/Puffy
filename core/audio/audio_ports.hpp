#pragma once

#include "core/audio/audio_types.hpp"

#include <functional>
#include <string>
#include <vector>

namespace puffy::audio {

struct AudioDeviceInfo {
    std::string id;
    std::string name;
    AudioFormat preferredFormat{};
    bool isDefault{false};
};

class IAudioCapture {
public:
    using Callback = std::function<void(AudioBlock)>;
    virtual ~IAudioCapture() = default;
    [[nodiscard]] virtual std::vector<AudioDeviceInfo> devices() const = 0;
    virtual bool open(const std::string& deviceId, AudioFormat format, Callback callback) = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept { return true; }
};

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    [[nodiscard]] virtual std::vector<AudioDeviceInfo> devices() const = 0;
    virtual bool open(const std::string& deviceId, AudioFormat format) = 0;
    virtual bool start() = 0;
    virtual void stop() noexcept = 0;
    virtual bool write(std::span<const float> interleavedSamples, std::size_t frames) noexcept = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept { return true; }
};

class IVirtualMicrophone {
public:
    virtual ~IVirtualMicrophone() = default;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() noexcept = 0;
    virtual bool pushAudio(std::span<const float> interleavedSamples,
                           std::size_t frames, int channels, int sampleRate) noexcept = 0;
};

} // namespace puffy::audio
