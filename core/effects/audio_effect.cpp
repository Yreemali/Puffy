#include "core/effects/audio_effect.hpp"

#include <algorithm>
#include <cmath>

namespace puffy::effects {

void Gain::process(std::span<float> samples, std::size_t, int) noexcept {
    for (auto& sample : samples) sample *= gain_;
}

void NoiseGate::process(std::span<float> samples, std::size_t frames, int channels) noexcept {
    const auto threshold = threshold_.load(std::memory_order_relaxed);
    const auto releaseSamples = releaseSamples_.load(std::memory_order_relaxed);
    for (std::size_t frame = 0; frame < frames; ++frame) {
        float peak = 0.0F;
        for (int channel = 0; channel < channels; ++channel) peak = std::max(peak, std::abs(samples[frame * static_cast<std::size_t>(channels) + channel]));
        if (peak >= threshold) remaining_ = releaseSamples;
        const float multiplier = remaining_ > 0 ? 1.0F : 0.0F;
        for (int channel = 0; channel < channels; ++channel) samples[frame * static_cast<std::size_t>(channels) + channel] *= multiplier;
        if (remaining_ > 0) --remaining_;
    }
}

void Limiter::process(std::span<float> samples, std::size_t, int) noexcept {
    const auto ceiling = ceiling_.load(std::memory_order_relaxed);
    for (auto& sample : samples) sample = std::clamp(sample, -ceiling, ceiling);
}

void Compressor::process(std::span<float> samples, std::size_t, int) noexcept {
    const auto threshold = threshold_.load(std::memory_order_relaxed);
    const auto ratio = ratio_.load(std::memory_order_relaxed);
    const auto attackCoefficient = std::exp(-1.0F / (sampleRate_ * std::max(attack_, 0.0001F)));
    const auto releaseCoefficient = std::exp(-1.0F / (sampleRate_ * std::max(release_, 0.0001F)));
    for (auto& sample : samples) {
        const auto magnitude = std::abs(sample);
        const auto coefficient = magnitude > envelope_ ? attackCoefficient : releaseCoefficient;
        envelope_ = coefficient * envelope_ + (1.0F - coefficient) * magnitude;
        float gain = 1.0F;
        if (envelope_ > threshold) {
            const auto compressed = threshold + (envelope_ - threshold) / ratio;
            gain = compressed / std::max(envelope_, 0.00001F);
        }
        sample *= gain;
    }
}

void OnePoleFilter::prepare(double sampleRate, std::size_t, int) noexcept {
    const auto normalized = std::clamp(cutoff_ / static_cast<float>(sampleRate), 0.0001F, 0.49F);
    coefficient_ = type_ == Type::LowPass ? 1.0F - std::exp(-2.0F * 3.14159265F * normalized)
                                           : std::exp(-2.0F * 3.14159265F * normalized);
}

void OnePoleFilter::process(std::span<float> samples, std::size_t frames, int channels) noexcept {
    if (channels <= 0 || channels > static_cast<int>(previous_.size())) return;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            auto& sample = samples[frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)];
            auto& previous = previous_[static_cast<std::size_t>(channel)];
            if (type_ == Type::LowPass) { previous += coefficient_ * (sample - previous); sample = previous; }
            else { const auto input = sample; const auto output = coefficient_ * (previous + input - previousInput_[static_cast<std::size_t>(channel)]); previousInput_[static_cast<std::size_t>(channel)] = input; previous = output; sample = output; }
        }
    }
}

void Delay::prepare(double sampleRate, std::size_t, int channels) noexcept {
    sampleRate_ = sampleRate; channels_ = std::clamp(channels, 1, 8);
    delaySamples_ = std::min<std::size_t>(maxDelaySamples_ - static_cast<std::size_t>(channels_),
        static_cast<std::size_t>(sampleRate_ * delayMilliseconds_ / 1000.0) * static_cast<std::size_t>(channels_));
}

void Delay::process(std::span<float> samples, std::size_t frames, int channels) noexcept {
    if (channels != channels_ || channels <= 0) return;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const auto index = writeIndex_ % maxDelaySamples_;
            const auto delayedIndex = (writeIndex_ + maxDelaySamples_ - delaySamples_) % maxDelaySamples_;
            auto& current = samples[frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)];
            const auto delayed = buffer_[delayedIndex];
            buffer_[index] = current;
            current = current * (1.0F - mix_) + delayed * mix_;
            ++writeIndex_;
        }
    }
}

void Delay::reset() noexcept { buffer_.fill(0.0F); writeIndex_ = 0; }

} // namespace puffy::effects
