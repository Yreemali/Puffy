#include "core/library/sound_cache.hpp"

#include <algorithm>
#include <limits>

namespace puffy::library {

SoundCache::SoundCache(std::size_t maxBytes) : maxBytes_(maxBytes) {}

std::size_t SoundCache::sizeOf(const audio::DecodedAudio& audio) noexcept {
    return audio.samples.size() * sizeof(float);
}

std::shared_ptr<const audio::DecodedAudio> SoundCache::load(
    const std::filesystem::path& path, const audio::IAudioDecoder& decoder, std::string& error) {
    if (const auto it = entries_.find(path); it != entries_.end()) {
        it->second.lastUsed = ++clock_;
        return it->second.audio;
    }

    auto decoded = decoder.decode(path, error);
    if (!decoded) return {};
    const auto bytes = sizeOf(*decoded);
    if (bytes > maxBytes_) { error = "Decoded sound exceeds cache size limit"; return {}; }
    entries_.insert_or_assign(path, Entry{decoded, bytes, ++clock_});
    bytesUsed_ += bytes;
    trim();
    return decoded;
}

void SoundCache::clear() noexcept {
    entries_.clear();
    bytesUsed_ = 0;
}

void SoundCache::setMaxBytes(std::size_t maxBytes) noexcept {
    maxBytes_ = maxBytes;
    trim();
}

void SoundCache::trim() noexcept {
    while (bytesUsed_ > maxBytes_ && !entries_.empty()) {
        auto oldest = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (oldest == entries_.end() || it->second.lastUsed < oldest->second.lastUsed) oldest = it;
        }
        bytesUsed_ -= oldest->second.bytes;
        entries_.erase(oldest);
    }
}

} // namespace puffy::library
