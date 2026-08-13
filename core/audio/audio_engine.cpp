#include "core/audio/audio_engine.hpp"

#include <algorithm>

namespace puffy::audio {

AudioEngine::AudioEngine(AudioEngineConfig config)
    : config_(config), mixer_(config.mixer),
      monitoringBuffer_(config.maxFrames * static_cast<std::size_t>(config.format.channels)),
      virtualBuffer_(config.maxFrames * static_cast<std::size_t>(config.format.channels)),
      microphoneBuffer_(config.maxFrames * static_cast<std::size_t>(config.format.channels)) {
    config_.mixer.maxFrames = config.maxFrames;
    config_.mixer.maxChannels = static_cast<std::size_t>(config.format.channels);
}

bool AudioEngine::start(IAudioCapture& capture, IAudioOutput& monitoringOutput,
                        IVirtualMicrophone* virtualMicrophone,
                        const std::string& captureId, const std::string& monitoringId) {
    if (running_) return false;
    state_ = State::Starting;
    if (!monitoringOutput.open(monitoringId, config_.format)) { state_ = State::Failed; return false; }
    if (!monitoringOutput.start()) { monitoringOutput.stop(); state_ = State::Failed; return false; }
    bool virtualReady = false;
    if (virtualMicrophone != nullptr) virtualReady = virtualMicrophone->initialize() && virtualMicrophone->start();
    capture_ = &capture;
    monitoringOutput_ = &monitoringOutput;
    virtualMicrophone_ = virtualMicrophone;
    captureId_ = captureId;
    monitoringId_ = monitoringId;
    running_ = true;
    const auto callback = [this](AudioBlock block) { processMicrophone(block); };
    // Physical microphones are commonly mono even when the processing graph
    // and outputs are stereo. Requesting stereo from PipeWire can make a
    // perfectly valid microphone fail to negotiate, which would also prevent
    // soundboard playback because capture drives the processing clock.
    auto captureFormat = config_.format;
    captureFormat.channels = 1;
    if (!capture.open(captureId, captureFormat, callback)) {
        if (virtualMicrophone_ != nullptr) virtualMicrophone_->stop();
        monitoringOutput_->stop();
        capture_ = nullptr; monitoringOutput_ = nullptr; virtualMicrophone_ = nullptr; running_ = false;
        state_ = State::Failed;
        return false;
    }
    if (!virtualReady) state_ = State::Degraded;
    else state_ = State::Running;
    return true;
}

bool AudioEngine::restart(const std::string& captureId, const std::string& monitoringId) {
    if (capture_ == nullptr || monitoringOutput_ == nullptr) return false;
    auto* capture = capture_;
    auto* output = monitoringOutput_;
    auto* virtualMicrophone = virtualMicrophone_;
    stop();
    return start(*capture, *output, virtualMicrophone, captureId, monitoringId);
}

void AudioEngine::stop() noexcept {
    if (capture_ != nullptr) capture_->close();
    if (virtualMicrophone_ != nullptr) virtualMicrophone_->stop();
    if (monitoringOutput_ != nullptr) monitoringOutput_->stop();
    capture_ = nullptr; monitoringOutput_ = nullptr; virtualMicrophone_ = nullptr; running_ = false; state_ = State::Stopped;
}

void AudioEngine::processMicrophone(AudioBlock microphone) noexcept {
    if (!running_ || microphone.channels <= 0) return;
    const auto frames = std::min({microphone.frames, config_.maxFrames,
                                  microphone.samples.size() / static_cast<std::size_t>(microphone.channels)});
    const auto channels = config_.format.channels;
    if (channels <= 0 || frames == 0) return;
    const auto sampleCount = frames * static_cast<std::size_t>(channels);
    drainCommands();

    // Convert the capture block to the engine's internal channel layout
    // without allocating. This handles mono microphones and stereo devices.
    for (std::size_t frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < channels; ++channel) {
            const auto sourceChannel = std::min<std::size_t>(
                static_cast<std::size_t>(channel),
                static_cast<std::size_t>(microphone.channels - 1));
            microphoneBuffer_[frame * static_cast<std::size_t>(channels) + static_cast<std::size_t>(channel)] =
                microphone.samples[frame * static_cast<std::size_t>(microphone.channels) + sourceChannel];
        }
    }
    microphoneEffects_.process(std::span<float>(microphoneBuffer_.data(), sampleCount), frames, channels);
    mixer_.process({std::span<const float>(microphoneBuffer_.data(), sampleCount), frames, channels}, monitoringBuffer_, virtualBuffer_, frames, channels);
    if (monitoringMuted_) std::fill_n(monitoringBuffer_.begin(), sampleCount, 0.0F);
    if (virtualMicrophoneMuted_) std::fill_n(virtualBuffer_.begin(), sampleCount, 0.0F);
    if (!monitoringMuted_ && monitoringOutput_ != nullptr)
        monitoringOutput_->write(std::span<const float>(monitoringBuffer_.data(), sampleCount), frames);
    if (!virtualMicrophoneMuted_ && virtualMicrophone_ != nullptr)
        virtualMicrophone_->pushAudio(std::span<const float>(virtualBuffer_.data(), sampleCount), frames, channels, config_.format.sampleRate);
}

bool AudioEngine::addMicrophoneEffect(effects::IAudioEffect& effect) noexcept {
    if (!microphoneEffects_.add(effect)) return false;
    effect.prepare(config_.format.sampleRate, config_.maxFrames, config_.format.channels);
    return true;
}

void AudioEngine::clearMicrophoneEffects() noexcept { microphoneEffects_.clear(); }

bool AudioEngine::registerSound(std::int64_t soundId, std::shared_ptr<const DecodedAudio> audio, bool loop, std::size_t fadeInFrames, std::size_t fadeOutFrames, float speed) {
    if (soundId == 0 || audio == nullptr || audio->channels <= 0 || audio->samples.empty()) return false;
    for (auto& sound : sounds_) {
        if (sound.used.load(std::memory_order_acquire) && sound.id == soundId) {
            if (running_) return true;
            sound.audio = std::move(audio); sound.loop = loop; sound.fadeInFrames = fadeInFrames; sound.fadeOutFrames = fadeOutFrames; sound.speed = speed; return true;
        }
    }
    for (auto& sound : sounds_) {
        if (!sound.used.load(std::memory_order_acquire)) {
            // Publish all immutable audio metadata before the realtime thread
            // can observe the slot as used.
            sound.id = soundId; sound.audio = std::move(audio); sound.loop = loop;
            sound.fadeInFrames = fadeInFrames; sound.fadeOutFrames = fadeOutFrames; sound.speed = speed;
            sound.used.store(true, std::memory_order_release);
            return true;
        }
    }
    return false;
}

bool AudioEngine::enqueue(Command command) noexcept {
    const auto write = commandWrite_.load(std::memory_order_relaxed);
    const auto read = commandRead_.load(std::memory_order_acquire);
    if (write - read >= commandCapacity_) return false;
    commands_[write % commandCapacity_] = command;
    commandWrite_.store(write + 1, std::memory_order_release);
    return true;
}

bool AudioEngine::playSound(std::int64_t soundId, OutputRoute route, float gain, bool loop, float speed, std::size_t fadeInFrames, std::size_t fadeOutFrames) noexcept {
    if (findSound(soundId) == nullptr || route == OutputRoute::None) return false;
    return enqueue({CommandType::Play, soundId, route, std::max(0.0F, gain), loop, std::max(0.05F, speed), fadeInFrames, fadeOutFrames});
}

bool AudioEngine::stopSound(std::int64_t soundId) noexcept {
    if (findSound(soundId) == nullptr) return false;
    return enqueue({CommandType::Stop, soundId, OutputRoute::None, 0.0F});
}

bool AudioEngine::stopAllSounds() noexcept { return enqueue({CommandType::StopAll, 0, OutputRoute::None, 0.0F}); }
bool AudioEngine::pauseAllSounds() noexcept { return enqueue({CommandType::Pause, 0, OutputRoute::None, 0.0F}); }
bool AudioEngine::resumeAllSounds() noexcept { return enqueue({CommandType::Resume, 0, OutputRoute::None, 0.0F}); }
bool AudioEngine::fadeOutAllSounds() noexcept { return enqueue({CommandType::FadeOut, 0, OutputRoute::None, 0.0F}); }

const DecodedAudio* AudioEngine::findSound(std::int64_t soundId) const noexcept {
    for (const auto& sound : sounds_) if (sound.used.load(std::memory_order_acquire) && sound.id == soundId) return sound.audio.get();
    return nullptr;
}

void AudioEngine::drainCommands() noexcept {
    auto read = commandRead_.load(std::memory_order_relaxed);
    const auto write = commandWrite_.load(std::memory_order_acquire);
    while (read != write) {
        const auto command = commands_[read % commandCapacity_];
        if (command.type == CommandType::StopAll) mixer_.clearVoices();
        else if (command.type == CommandType::Pause) { paused_ = true; mixer_.setVoicesPaused(true); }
        else if (command.type == CommandType::Resume) { paused_ = false; mixer_.setVoicesPaused(false); }
        else if (command.type == CommandType::FadeOut) mixer_.clearVoices();
        else if (command.type == CommandType::Stop) mixer_.stopSource(command.soundId);
        else if (const auto* sound = findSound(command.soundId); sound != nullptr) {
            for (const auto& registered : sounds_) if (registered.used && registered.audio.get() == sound) {
                mixer_.addVoice({sound->samples, sound->frames(), sound->channels}, command.gain, command.route, command.soundId, command.loop || registered.loop, command.fadeInFrames > 0 ? command.fadeInFrames : registered.fadeInFrames, command.fadeOutFrames > 0 ? command.fadeOutFrames : registered.fadeOutFrames, command.speed); break;
            }
        }
        ++read;
    }
    commandRead_.store(read, std::memory_order_release);
}

void AudioEngine::setMicrophoneGain(float gain) noexcept { mixer_.setMicrophoneGain(std::max(0.0F, gain)); }
void AudioEngine::setSoundboardGain(float gain) noexcept { mixer_.setSoundboardGain(std::max(0.0F, gain)); }
void AudioEngine::setMonitoringGain(float gain) noexcept { mixer_.setMonitoringGain(std::max(0.0F, gain)); }
void AudioEngine::setVirtualOutputGain(float gain) noexcept { mixer_.setVirtualOutputGain(std::max(0.0F, gain)); }
void AudioEngine::setMasterCeiling(float ceiling) noexcept { mixer_.setMasterCeiling(std::clamp(ceiling, 0.01F, 1.0F)); }
void AudioEngine::setMonitorMicrophone(bool enabled) noexcept { mixer_.setMonitorMicrophone(enabled); }
void AudioEngine::setMonitoringMuted(bool muted) noexcept { monitoringMuted_ = muted; }
void AudioEngine::setVirtualMicrophoneMuted(bool muted) noexcept { virtualMicrophoneMuted_ = muted; }

bool AudioEngine::devicesHealthy() const noexcept {
    return running_ && capture_ != nullptr && monitoringOutput_ != nullptr && capture_->isOpen() && monitoringOutput_->isOpen();
}

AudioEngine::LatencyEstimate AudioEngine::latency() const noexcept {
    const auto rate = static_cast<double>(std::max(1, config_.format.sampleRate));
    const auto bufferMs = static_cast<double>(config_.bufferFrames) * 1000.0 / rate;
    return {bufferMs, bufferMs, bufferMs, bufferMs * 3.0, true};
}

} // namespace puffy::audio
