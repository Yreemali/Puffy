#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace puffy::audio {

struct DecodedAudio {
    int sampleRate{0};
    int channels{0};
    std::vector<float> samples;

    [[nodiscard]] std::size_t frames() const noexcept {
        return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0;
    }
};

class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;
    [[nodiscard]] virtual std::shared_ptr<const DecodedAudio> decode(
        const std::filesystem::path& path,
        std::string& error) const = 0;
};

class WavDecoder final : public IAudioDecoder {
public:
    [[nodiscard]] std::shared_ptr<const DecodedAudio> decode(
        const std::filesystem::path& path,
        std::string& error) const override;
};

class SndFileDecoder final : public IAudioDecoder {
public:
    [[nodiscard]] std::shared_ptr<const DecodedAudio> decode(
        const std::filesystem::path& path,
        std::string& error) const override;
};

} // namespace puffy::audio
