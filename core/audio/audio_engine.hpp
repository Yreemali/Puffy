#pragma once

#include "core/audio/audio_ports.hpp"
#include "core/audio/audio_decoder.hpp"
#include "core/mixer/mixer.hpp"
#include "core/effects/effects_chain.hpp"

#include <memory>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace puffy::audio {

struct AudioEngineConfig {
    AudioFormat format{48000, 2};
    std::size_t maxFrames{2048};
    std::size_t bufferFrames{128};
    mixer::MixerConfig mixer{};
};

class AudioEngine final {
public:
    enum class State { Stopped, Starting, Running, Degraded, Failed };
    explicit AudioEngine(AudioEngineConfig config = {});

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Device startup and shutdown happen outside the realtime callback.
    bool start(IAudioCapture& capture,
               IAudioOutput& monitoringOutput,
               IVirtualMicrophone* virtualMicrophone,
               const std::string& captureId = "default",
               const std::string& monitoringId = "default");
    void stop() noexcept;
    bool restart(const std::string& captureId, const std::string& monitoringId);

    // Called synchronously by the capture callback. No allocations or locks.
    void processMicrophone(AudioBlock microphone) noexcept;

    // Register decoded buffers before start(). The registry must not be mutated
    // while the engine is running; this keeps realtime lookup allocation-free.
    bool registerSound(std::int64_t soundId, std::shared_ptr<const DecodedAudio> audio, bool loop = false, std::size_t fadeInFrames = 0, std::size_t fadeOutFrames = 0, float speed = 1.0F);
    bool playSound(std::int64_t soundId, OutputRoute route, float gain = 1.0F, bool loop = false, float speed = 1.0F, std::size_t fadeInFrames = 0, std::size_t fadeOutFrames = 0) noexcept;
    bool stopSound(std::int64_t soundId) noexcept;
    bool stopAllSounds() noexcept;
    bool pauseAllSounds() noexcept;
    bool resumeAllSounds() noexcept;
    bool fadeOutAllSounds() noexcept;
    bool addMicrophoneEffect(effects::IAudioEffect& effect) noexcept;
    void clearMicrophoneEffects() noexcept;

    [[nodiscard]] mixer::Mixer& mixer() noexcept { return mixer_; }
    void setMicrophoneGain(float gain) noexcept;
    void setSoundboardGain(float gain) noexcept;
    void setMonitoringGain(float gain) noexcept;
    void setVirtualOutputGain(float gain) noexcept;
    void setMasterGain(float gain) noexcept;
    void setMasterCeiling(float ceiling) noexcept;
    void setMonitorMicrophone(bool enabled) noexcept;
    void setMonitoringMuted(bool muted) noexcept;
    void setVirtualMicrophoneMuted(bool muted) noexcept;
    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] bool hasMonitoringOutput() const noexcept { return monitoringOutput_ != nullptr; }
    [[nodiscard]] bool hasVirtualMicrophone() const noexcept { return virtualMicrophone_ != nullptr; }
    [[nodiscard]] bool devicesHealthy() const noexcept;
    struct LatencyEstimate { double inputMs{0}; double processingMs{0}; double outputMs{0}; double totalMs{0}; bool estimated{true}; };
    [[nodiscard]] LatencyEstimate latency() const noexcept;
    void setBufferFrames(std::size_t frames) noexcept { config_.bufferFrames = frames; }

private:
    enum class CommandType : std::uint8_t { Play, Stop, StopAll, Pause, Resume, FadeOut };
    struct Command { CommandType type{CommandType::Play}; std::int64_t soundId{0}; OutputRoute route{OutputRoute::Both}; float gain{1.0F}; bool loop{false}; float speed{1.0F}; std::size_t fadeInFrames{0}; std::size_t fadeOutFrames{0}; };
    struct RegisteredSound {
        std::int64_t id{0};
        std::shared_ptr<const DecodedAudio> audio;
        bool loop{false};
        std::size_t fadeInFrames{0};
        std::size_t fadeOutFrames{0};
        float speed{1.0F};
        std::atomic<bool> used{false};
    };
    static constexpr std::size_t commandCapacity_ = 128;
    static constexpr std::size_t soundCapacity_ = 256;

    bool enqueue(Command command) noexcept;
    void drainCommands() noexcept;
    [[nodiscard]] const DecodedAudio* findSound(std::int64_t soundId) const noexcept;

    AudioEngineConfig config_;
    mixer::Mixer mixer_;
    IAudioCapture* capture_{nullptr};
    IAudioOutput* monitoringOutput_{nullptr};
    IVirtualMicrophone* virtualMicrophone_{nullptr};
    std::vector<float> monitoringBuffer_;
    std::vector<float> virtualBuffer_;
    std::vector<float> microphoneBuffer_;
    effects::EffectsChain microphoneEffects_;
    bool monitoringMuted_{false};
    bool virtualMicrophoneMuted_{false};
    bool running_{false};
    bool paused_{false};
    State state_{State::Stopped};
    std::string captureId_;
    std::string monitoringId_;
    std::array<Command, commandCapacity_> commands_{};
    std::atomic<std::uint32_t> commandWrite_{0};
    std::atomic<std::uint32_t> commandRead_{0};
    std::array<RegisteredSound, soundCapacity_> sounds_{};
};

} // namespace puffy::audio
