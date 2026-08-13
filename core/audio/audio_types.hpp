#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace puffy::audio {

enum class OutputRoute : std::uint8_t {
    None = 0,
    Headphones = 1,
    VirtualMicrophone = 2,
    Both = 3,
};

constexpr OutputRoute operator|(OutputRoute lhs, OutputRoute rhs) noexcept {
    return static_cast<OutputRoute>(static_cast<std::uint8_t>(lhs) |
                                    static_cast<std::uint8_t>(rhs));
}

constexpr bool contains(OutputRoute value, OutputRoute flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

struct AudioFormat {
    int sampleRate{48'000};
    int channels{2};
};

struct AudioBlock {
    std::span<const float> samples{};
    std::size_t frames{0};
    int channels{0};

    [[nodiscard]] bool valid() const noexcept {
        return channels > 0 && frames > 0 && samples.size() >= frames * static_cast<std::size_t>(channels);
    }
};

} // namespace puffy::audio
