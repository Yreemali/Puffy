#pragma once

#include "core/effects/audio_effect.hpp"

#include <array>
#include <memory>

namespace puffy::effects {

class EffectsChain final {
public:
    static constexpr std::size_t maxEffects = 16;
    bool add(IAudioEffect& effect) noexcept;
    void clear() noexcept;
    void prepare(double sampleRate, std::size_t maxFrames, int channels) noexcept;
    void process(std::span<float> samples, std::size_t frames, int channels) noexcept;
    void reset() noexcept;
private:
    std::array<IAudioEffect*, maxEffects> effects_{};
    std::size_t count_{0};
};

} // namespace puffy::effects
