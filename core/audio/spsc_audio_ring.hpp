#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <span>
#include <vector>

namespace puffy::audio {

/** Bounded single-producer/single-consumer float queue for device callbacks. */
class SpscAudioRing final {
public:
    explicit SpscAudioRing(std::size_t sampleCapacity)
        : storage_(std::max<std::size_t>(sampleCapacity, 2U) + 1U) {}

    SpscAudioRing(const SpscAudioRing&) = delete;
    SpscAudioRing& operator=(const SpscAudioRing&) = delete;

    [[nodiscard]] std::size_t capacity() const noexcept { return storage_.size() - 1U; }

    [[nodiscard]] bool write(std::span<const float> samples) noexcept {
        if (samples.empty()) return true;
        const auto read = read_.load(std::memory_order_acquire);
        const auto write = write_.load(std::memory_order_relaxed);
        const auto used = distance(read, write);
        if (samples.size() > capacity() - used) return false;
        copyIn(write, samples);
        write_.store(advance(write, samples.size()), std::memory_order_release);
        return true;
    }

    std::size_t read(std::span<float> destination) noexcept {
        if (destination.empty()) return 0;
        const auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_acquire);
        const auto count = std::min(destination.size(), distance(read, write));
        copyOut(read, destination.first(count));
        read_.store(advance(read, count), std::memory_order_release);
        return count;
    }

    void clear() noexcept {
        read_.store(write_.load(std::memory_order_acquire), std::memory_order_release);
    }

private:
    [[nodiscard]] std::size_t advance(std::size_t index, std::size_t count) const noexcept {
        return (index + count) % storage_.size();
    }

    [[nodiscard]] std::size_t distance(std::size_t begin, std::size_t end) const noexcept {
        return end >= begin ? end - begin : storage_.size() - begin + end;
    }

    void copyIn(std::size_t index, std::span<const float> source) noexcept {
        const auto first = std::min(source.size(), storage_.size() - index);
        std::copy_n(source.data(), first, storage_.data() + index);
        std::copy_n(source.data() + first, source.size() - first, storage_.data());
    }

    void copyOut(std::size_t index, std::span<float> destination) noexcept {
        const auto first = std::min(destination.size(), storage_.size() - index);
        std::copy_n(storage_.data() + index, first, destination.data());
        std::copy_n(storage_.data(), destination.size() - first, destination.data() + first);
    }

    std::vector<float> storage_;
    alignas(64) std::atomic<std::size_t> read_{0};
    alignas(64) std::atomic<std::size_t> write_{0};
};

} // namespace puffy::audio
