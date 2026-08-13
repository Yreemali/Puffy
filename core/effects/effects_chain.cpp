#include "core/effects/effects_chain.hpp"

namespace puffy::effects {

bool EffectsChain::add(IAudioEffect& effect) noexcept {
    if (count_ >= maxEffects) return false;
    effects_[count_++] = &effect;
    return true;
}

void EffectsChain::clear() noexcept { count_ = 0; }

void EffectsChain::prepare(double sampleRate, std::size_t maxFrames, int channels) noexcept {
    for (std::size_t i = 0; i < count_; ++i) effects_[i]->prepare(sampleRate, maxFrames, channels);
}

void EffectsChain::process(std::span<float> samples, std::size_t frames, int channels) noexcept {
    for (std::size_t i = 0; i < count_; ++i) effects_[i]->process(samples, frames, channels);
}

void EffectsChain::reset() noexcept { for (std::size_t i = 0; i < count_; ++i) effects_[i]->reset(); }

} // namespace puffy::effects
