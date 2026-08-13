#include "platform/linux/pipewire_output.hpp"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>

#include <algorithm>
#include <cstring>

namespace puffy::platform::linux {

struct PipeWireOutput::State {
    pw_thread_loop* threadLoop{nullptr};
    pw_stream* stream{nullptr};
    spa_ringbuffer ring{SPA_RINGBUFFER_INIT()};
    std::vector<std::byte> ringStorage;
    audio::AudioFormat format{};
    bool open{false};
    bool started{false};
};

namespace {

void onProcess(void* userData) {
    auto& state = *static_cast<PipeWireOutput::State*>(userData);
    auto* buffer = pw_stream_dequeue_buffer(state.stream);
    if (buffer == nullptr || buffer->buffer->n_datas == 0) return;
    auto& data = buffer->buffer->datas[0];
    auto* destination = static_cast<float*>(data.data);
    if (destination == nullptr || data.maxsize == 0) {
        pw_stream_queue_buffer(state.stream, buffer);
        return;
    }

    const auto requestedBytes = std::min<std::size_t>(data.maxsize, state.ringStorage.size());
    const auto requestedSamples = requestedBytes / sizeof(float);
    const auto requestedFrames = requestedSamples / static_cast<std::size_t>(state.format.channels);
    const auto outputBytes = requestedFrames * static_cast<std::size_t>(state.format.channels) * sizeof(float);
    std::uint32_t readIndex = 0;
    const auto available = spa_ringbuffer_get_read_index(&state.ring, &readIndex);
    const auto readable = std::min<std::size_t>(outputBytes, available > 0 ? static_cast<std::size_t>(available) : 0U);
    spa_ringbuffer_read_data(&state.ring, state.ringStorage.data(), static_cast<std::uint32_t>(state.ringStorage.size()),
                             readIndex % static_cast<std::uint32_t>(state.ringStorage.size()), destination,
                             static_cast<std::uint32_t>(readable));
    if (readable < outputBytes) std::memset(reinterpret_cast<std::byte*>(destination) + readable, 0, outputBytes - readable);
    spa_ringbuffer_read_update(&state.ring, static_cast<std::int32_t>(readIndex + readable));
    data.chunk->offset = 0;
    data.chunk->size = static_cast<std::uint32_t>(outputBytes);
    data.chunk->stride = static_cast<int32_t>(state.format.channels * sizeof(float));
    pw_stream_queue_buffer(state.stream, buffer);
}

constexpr pw_stream_events events{
    PW_VERSION_STREAM_EVENTS, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, onProcess, nullptr, nullptr, nullptr,
};

} // namespace

PipeWireOutput::PipeWireOutput(std::size_t ringBufferBytes) : state_(std::make_unique<State>()) {
    const auto aligned = std::max<std::size_t>(ringBufferBytes & ~std::size_t{sizeof(float) - 1U}, sizeof(float) * 2U);
    state_->ringStorage.resize(aligned);
    spa_ringbuffer_init(&state_->ring);
}

PipeWireOutput::~PipeWireOutput() { stop(); }

std::vector<audio::AudioDeviceInfo> PipeWireOutput::devices() const {
    // pw_stream_new_simple delegates device selection to the PipeWire session manager.
    return {{"default", "PipeWire default output", {48000, 2}, true}};
}

bool PipeWireOutput::open(const std::string& deviceId, audio::AudioFormat format) {
    if (state_->open || format.channels <= 0 || format.sampleRate <= 0) return false;
    state_->format = format;
    pw_init(nullptr, nullptr);
    state_->threadLoop = pw_thread_loop_new("puffy-pipewire-output", nullptr);
    if (state_->threadLoop == nullptr) return false;
    if (pw_thread_loop_start(state_->threadLoop) < 0) { pw_thread_loop_destroy(state_->threadLoop); state_->threadLoop = nullptr; return false; }

    pw_thread_loop_lock(state_->threadLoop);
    auto* properties = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                         PW_KEY_MEDIA_CATEGORY, "Playback",
                                         PW_KEY_MEDIA_ROLE, "Game",
                                         PW_KEY_NODE_LATENCY, "128/48000",
                                         PW_KEY_NODE_FORCE_QUANTUM, "128", nullptr);
    if (!deviceId.empty() && deviceId != "default") pw_properties_set(properties, PW_KEY_TARGET_OBJECT, deviceId.c_str());
    state_->stream = pw_stream_new_simple(pw_thread_loop_get_loop(state_->threadLoop), "puffy-monitoring", properties, &events, state_.get());
    std::uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw raw = SPA_AUDIO_INFO_RAW_INIT();
    raw.format = SPA_AUDIO_FORMAT_F32;
    raw.rate = static_cast<uint32_t>(format.sampleRate);
    raw.channels = static_cast<uint32_t>(format.channels);
    const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat,
        &raw)};
    const auto result = state_->stream == nullptr ? -1 : pw_stream_connect(state_->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS), params, 1);
    pw_thread_loop_unlock(state_->threadLoop);
    if (result < 0) { stop(); return false; }
    state_->open = true;
    return true;
}

bool PipeWireOutput::start() { return state_->open && (state_->started = true); }

void PipeWireOutput::stop() noexcept {
    if (state_->threadLoop == nullptr) return;
    pw_thread_loop_lock(state_->threadLoop);
    if (state_->stream != nullptr) { pw_stream_disconnect(state_->stream); pw_stream_destroy(state_->stream); state_->stream = nullptr; }
    pw_thread_loop_unlock(state_->threadLoop);
    pw_thread_loop_stop(state_->threadLoop);
    pw_thread_loop_destroy(state_->threadLoop);
    state_->threadLoop = nullptr; state_->open = false; state_->started = false;
}

bool PipeWireOutput::write(std::span<const float> samples, std::size_t frames) noexcept {
    if (!state_->started || state_->format.channels <= 0 || samples.size() < frames * static_cast<std::size_t>(state_->format.channels)) return false;
    const auto bytes = frames * static_cast<std::size_t>(state_->format.channels) * sizeof(float);
    if (bytes > state_->ringStorage.size()) return false;
    std::uint32_t writeIndex = 0;
    const auto used = spa_ringbuffer_get_write_index(&state_->ring, &writeIndex);
    const auto freeBytes = state_->ringStorage.size() - std::min<std::size_t>(state_->ringStorage.size(), used > 0 ? static_cast<std::size_t>(used) : 0U);
    if (freeBytes < bytes) {
        // Prefer the newest audio. Drop the oldest samples instead of
        // accumulating stale sound and producing audible multi-second lag.
        const auto newRead = writeIndex + static_cast<std::uint32_t>(bytes)
            - static_cast<std::uint32_t>(state_->ringStorage.size());
        spa_ringbuffer_read_update(&state_->ring, static_cast<std::int32_t>(newRead));
    }
    spa_ringbuffer_write_data(&state_->ring, state_->ringStorage.data(), static_cast<std::uint32_t>(state_->ringStorage.size()),
                              writeIndex % static_cast<std::uint32_t>(state_->ringStorage.size()), samples.data(), static_cast<std::uint32_t>(bytes));
    spa_ringbuffer_write_update(&state_->ring, static_cast<std::int32_t>(writeIndex + bytes));
    return true;
}

bool PipeWireOutput::isOpen() const noexcept { return state_->open && state_->stream != nullptr; }

} // namespace puffy::platform::linux
