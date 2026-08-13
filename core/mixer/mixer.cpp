#include "core/mixer/mixer.hpp"

#include <algorithm>
#include <cmath>

namespace puffy::mixer {

Mixer::Mixer(MixerConfig config) : config_(config), voices_(config.maxVoices) {}

void Mixer::clearVoices() noexcept {
    for (auto& voice : voices_) voice.active = false;
}

bool Mixer::addVoice(audio::AudioBlock block, float gain, audio::OutputRoute route, std::int64_t sourceId, bool loop, std::size_t fadeInFrames, std::size_t fadeOutFrames, float speed) noexcept {
    if (!block.valid() || route == audio::OutputRoute::None) return false;
    for (auto& voice : voices_) {
        if (!voice.active) {
            voice = Voice{block, gain, route, sourceId, loop, fadeInFrames, fadeOutFrames, std::max(0.05F, speed), 0.0, true};
            return true;
        }
    }
    return false;
}

void Mixer::stopSource(std::int64_t sourceId) noexcept {
    for (auto& voice : voices_) {
        if (voice.active && voice.sourceId == sourceId) voice.active = false;
    }
}

void Mixer::process(audio::AudioBlock microphone,
                    std::span<float> monitoringOutput,
                    std::span<float> virtualOutput,
                    std::size_t frames,
                    int outputChannels) noexcept {
    const auto sampleCount = frames * static_cast<std::size_t>(outputChannels);
    if (outputChannels <= 0 || monitoringOutput.size() < sampleCount || virtualOutput.size() < sampleCount) return;
    std::fill_n(monitoringOutput.begin(), sampleCount, 0.0F);
    std::fill_n(virtualOutput.begin(), sampleCount, 0.0F);

    auto add = [outputChannels, frames](std::span<float> destination, audio::AudioBlock source,
                                        float gain, double position, float speed) {
        if (!source.valid()) return;
        const auto sourceChannels = static_cast<std::size_t>(source.channels);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const auto sourcePosition = position + static_cast<double>(frame) * speed;
            if (sourcePosition >= static_cast<double>(source.frames)) break;
            const auto sourceFrame = static_cast<std::size_t>(sourcePosition);
            const auto nextFrame = std::min(sourceFrame + 1, source.frames - 1);
            const auto fraction = static_cast<float>(sourcePosition - static_cast<double>(sourceFrame));
            for (int channel = 0; channel < outputChannels; ++channel) {
                const auto sourceChannel = std::min<std::size_t>(static_cast<std::size_t>(channel), sourceChannels - 1);
                const auto first = source.samples[sourceFrame * sourceChannels + sourceChannel];
                const auto second = source.samples[nextFrame * sourceChannels + sourceChannel];
                destination[frame * static_cast<std::size_t>(outputChannels) + static_cast<std::size_t>(channel)] +=
                    (first + (second - first) * fraction) * gain;
            }
        }
    };

    if (microphone.valid()) {
        add(monitoringOutput, microphone, config_.microphoneGain * config_.monitoringGain, 0.0, 1.0F);
        add(virtualOutput, microphone, config_.microphoneGain * config_.virtualOutputGain, 0.0, 1.0F);
    }
    for (auto& voice : voices_) {
        if (!voice.active) continue;
        if (voicesPaused_) continue;
        const auto remaining = voice.block.frames > static_cast<std::size_t>(voice.position) ? voice.block.frames - static_cast<std::size_t>(voice.position) : 0U;
        const auto fadeFrames = std::max<std::size_t>(1, voice.fadeOutFrames);
        const auto fadeMultiplier = voice.fadeInFrames > static_cast<std::size_t>(voice.position)
            ? static_cast<float>(voice.position) / static_cast<float>(std::max<std::size_t>(1, voice.fadeInFrames))
            : (voice.fadeOutFrames > 0 && remaining <= fadeFrames
                ? static_cast<float>(remaining) / static_cast<float>(fadeFrames) : 1.0F);
        if (audio::contains(voice.route, audio::OutputRoute::Headphones))
            add(monitoringOutput, voice.block, voice.gain * config_.soundboardGain * fadeMultiplier, voice.position, voice.speed);
        if (audio::contains(voice.route, audio::OutputRoute::VirtualMicrophone))
            add(virtualOutput, voice.block, voice.gain * config_.soundboardGain * fadeMultiplier, voice.position, voice.speed);
        voice.position += static_cast<double>(frames) * voice.speed;
        if (voice.position >= static_cast<double>(voice.block.frames)) {
            if (voice.loop) voice.position = std::fmod(voice.position, static_cast<double>(voice.block.frames));
            else voice.active = false;
        }
    }
    for (std::size_t index = 0; index < sampleCount; ++index) {
        monitoringOutput[index] = std::clamp(monitoringOutput[index], -config_.masterCeiling, config_.masterCeiling);
        virtualOutput[index] = std::clamp(virtualOutput[index], -config_.masterCeiling, config_.masterCeiling);
    }
}

} // namespace puffy::mixer
