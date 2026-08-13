#pragma once

#include "core/audio/audio_types.hpp"

#include <cstddef>
#include <atomic>
#include <vector>

namespace puffy::mixer {

struct MixerConfig {
    std::size_t maxFrames{2'048};
    std::size_t maxChannels{2};
    std::size_t maxVoices{32};
    float microphoneGain{1.0F};
    float soundboardGain{1.0F};
    float monitoringGain{1.0F};
    float virtualOutputGain{1.0F};
    float masterGain{1.0F};
    float masterCeiling{0.98F};
    bool monitorMicrophone{false};
};

class Mixer final {
public:
    struct Levels {
        float microphone{0.0F};
        float soundboard{0.0F};
        float monitoring{0.0F};
        float virtualOutput{0.0F};
    };

    explicit Mixer(MixerConfig config = {});

    void clearVoices() noexcept;
    bool addVoice(audio::AudioBlock block, float gain, audio::OutputRoute route, std::int64_t sourceId = 0, bool loop = false, std::size_t fadeInFrames = 0, std::size_t fadeOutFrames = 0, float speed = 1.0F) noexcept;
    void stopSource(std::int64_t sourceId) noexcept;
    void setVoicesPaused(bool paused) noexcept { voicesPaused_ = paused; }
    void setMicrophoneGain(float gain) noexcept { config_.microphoneGain = gain; }
    void setSoundboardGain(float gain) noexcept { config_.soundboardGain = gain; }
    void setMonitoringGain(float gain) noexcept { config_.monitoringGain = gain; }
    void setVirtualOutputGain(float gain) noexcept { config_.virtualOutputGain = gain; }
    void setMasterGain(float gain) noexcept { config_.masterGain = gain; }
    void setMasterCeiling(float ceiling) noexcept { config_.masterCeiling = ceiling; }
    void setMonitorMicrophone(bool enabled) noexcept { config_.monitorMicrophone = enabled; }
    [[nodiscard]] Levels levels() const noexcept;

    // Mixes the current microphone block and active voices into preallocated outputs.
    // This function does not allocate or lock and is intended for the audio thread.
    void process(audio::AudioBlock microphone,
                 std::span<float> monitoringOutput,
                 std::span<float> virtualOutput,
                 std::size_t frames,
                 int outputChannels) noexcept;

private:
    struct Voice {
        audio::AudioBlock block{};
        float gain{1.0F};
        audio::OutputRoute route{audio::OutputRoute::None};
        std::int64_t sourceId{0};
        bool loop{false};
        std::size_t fadeInFrames{0};
        std::size_t fadeOutFrames{0};
        float speed{1.0F};
        double position{0.0};
        bool active{false};
    };

    MixerConfig config_;
    std::vector<Voice> voices_;
    bool voicesPaused_{false};
    std::atomic<float> microphoneLevel_{0.0F};
    std::atomic<float> soundboardLevel_{0.0F};
    std::atomic<float> monitoringLevel_{0.0F};
    std::atomic<float> virtualOutputLevel_{0.0F};
};

} // namespace puffy::mixer
