#pragma once

#include <cstddef>
#include <array>
#include <algorithm>
#include <span>

namespace puffy::effects {

class IAudioEffect {
public:
    virtual ~IAudioEffect() = default;
    virtual void prepare(double sampleRate, std::size_t maxFrames, int channels) noexcept = 0;
    virtual void process(std::span<float> interleaved, std::size_t frames, int channels) noexcept = 0;
    virtual void reset() noexcept = 0;
};

class Gain final : public IAudioEffect {
public:
    explicit Gain(float gain = 1.0F) noexcept : gain_(gain) {}
    void setGain(float gain) noexcept { gain_ = gain; }
    void prepare(double, std::size_t, int) noexcept override {}
    void process(std::span<float> samples, std::size_t, int) noexcept override;
    void reset() noexcept override {}
private:
    float gain_;
};

class NoiseGate final : public IAudioEffect {
public:
    void setThreshold(float threshold) noexcept { threshold_ = threshold; }
    void setReleaseSamples(std::size_t samples) noexcept { releaseSamples_ = samples; }
    void prepare(double, std::size_t, int) noexcept override {}
    void process(std::span<float> samples, std::size_t frames, int channels) noexcept override;
    void reset() noexcept override { remaining_ = 0; }
private:
    float threshold_{0.015F};
    std::size_t releaseSamples_{2400};
    std::size_t remaining_{0};
};

class Limiter final : public IAudioEffect {
public:
    void setCeiling(float ceiling) noexcept { ceiling_ = ceiling; }
    void prepare(double, std::size_t, int) noexcept override {}
    void process(std::span<float> samples, std::size_t, int) noexcept override;
    void reset() noexcept override {}
private:
    float ceiling_{0.98F};
};

class Compressor final : public IAudioEffect {
public:
    void setThreshold(float value) noexcept { threshold_ = value; }
    void setRatio(float value) noexcept { ratio_ = value < 1.0F ? 1.0F : value; }
    void setAttackRelease(float attack, float release) noexcept { attack_ = attack; release_ = release; }
    void prepare(double sampleRate, std::size_t, int) noexcept override { sampleRate_ = static_cast<float>(sampleRate); }
    void process(std::span<float> samples, std::size_t, int) noexcept override;
    void reset() noexcept override { envelope_ = 0.0F; }
private:
    float threshold_{0.5F};
    float ratio_{4.0F};
    float attack_{0.01F};
    float release_{0.1F};
    float sampleRate_{48000.0F};
    float envelope_{0.0F};
};

class OnePoleFilter final : public IAudioEffect {
public:
    enum class Type { LowPass, HighPass };
    explicit OnePoleFilter(Type type) noexcept : type_(type) {}
    void setCutoff(float value) noexcept { cutoff_ = value; }
    void prepare(double sampleRate, std::size_t, int channels) noexcept override;
    void process(std::span<float> samples, std::size_t frames, int channels) noexcept override;
    void reset() noexcept override { previous_.fill(0.0F); previousInput_.fill(0.0F); }
private:
    Type type_;
    float cutoff_{1000.0F};
    float coefficient_{0.1F};
    std::array<float, 8> previous_{};
    std::array<float, 8> previousInput_{};
};

class Delay final : public IAudioEffect {
public:
    void setDelayMilliseconds(float value) noexcept { delayMilliseconds_ = value; }
    void setMix(float value) noexcept { mix_ = std::clamp(value, 0.0F, 1.0F); }
    void prepare(double sampleRate, std::size_t maxFrames, int channels) noexcept override;
    void process(std::span<float> samples, std::size_t frames, int channels) noexcept override;
    void reset() noexcept override;
private:
    static constexpr std::size_t maxDelaySamples_ = 48000U * 2U;
    std::array<float, maxDelaySamples_> buffer_{};
    std::size_t writeIndex_{0};
    std::size_t delaySamples_{2400};
    float delayMilliseconds_{50.0F};
    float mix_{0.2F};
    int channels_{2};
    double sampleRate_{48000.0};
};

} // namespace puffy::effects
