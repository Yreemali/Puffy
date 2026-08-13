#pragma once

#include "core/audio/audio_decoder.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace puffy::library {

class SoundCache final {
public:
    explicit SoundCache(std::size_t maxBytes = 256U * 1024U * 1024U);

    // Loading is synchronous by design and must run on a decode worker, never
    // from an audio callback. Returned audio is immutable and thread-safe to read.
    [[nodiscard]] std::shared_ptr<const audio::DecodedAudio> load(
        const std::filesystem::path& path,
        const audio::IAudioDecoder& decoder,
        std::string& error);

    void clear() noexcept;
    void trim() noexcept;
    [[nodiscard]] std::size_t bytesUsed() const noexcept { return bytesUsed_; }
    [[nodiscard]] std::size_t maxBytes() const noexcept { return maxBytes_; }
    void setMaxBytes(std::size_t maxBytes) noexcept;

private:
    struct Entry {
        std::shared_ptr<const audio::DecodedAudio> audio;
        std::size_t bytes{0};
        std::uint64_t lastUsed{0};
    };

    static std::size_t sizeOf(const audio::DecodedAudio& audio) noexcept;
    std::size_t maxBytes_;
    std::size_t bytesUsed_{0};
    std::uint64_t clock_{0};
    std::unordered_map<std::filesystem::path, Entry> entries_;
};

} // namespace puffy::library
