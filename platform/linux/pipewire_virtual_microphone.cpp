#include "platform/linux/pipewire_virtual_microphone.hpp"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/ringbuffer.h>

#include <algorithm>
#include <cstring>

namespace puffy::platform::linux {

struct PipeWireVirtualMicrophone::State {
    pw_thread_loop* loop{nullptr};
    pw_stream* stream{nullptr};
    spa_ringbuffer ring{SPA_RINGBUFFER_INIT()};
    std::vector<std::byte> storage;
    audio::AudioFormat format{};
    bool initialized{false};
    bool started{false};
};

namespace {

void onProcess(void* userData) {
    auto& state = *static_cast<PipeWireVirtualMicrophone::State*>(userData);
    auto* buffer = pw_stream_dequeue_buffer(state.stream);
    if (buffer == nullptr || buffer->buffer->n_datas == 0) return;
    auto& data = buffer->buffer->datas[0];
    auto* destination = static_cast<float*>(data.data);
    if (destination == nullptr || data.maxsize == 0) {
        pw_stream_queue_buffer(state.stream, buffer);
        return;
    }
    const auto bytes = std::min<std::size_t>(data.maxsize, state.storage.size());
    std::uint32_t read = 0;
    const auto available = spa_ringbuffer_get_read_index(&state.ring, &read);
    const auto readable = std::min<std::size_t>(bytes, available > 0 ? static_cast<std::size_t>(available) : 0U);
    spa_ringbuffer_read_data(&state.ring, state.storage.data(), static_cast<std::uint32_t>(state.storage.size()),
                             read % static_cast<std::uint32_t>(state.storage.size()), destination,
                             static_cast<std::uint32_t>(readable));
    if (readable < bytes) std::memset(reinterpret_cast<std::byte*>(destination) + readable, 0, bytes - readable);
    spa_ringbuffer_read_update(&state.ring, static_cast<std::int32_t>(read + readable));
    data.chunk->offset = 0;
    data.chunk->size = static_cast<std::uint32_t>(bytes);
    data.chunk->stride = state.format.channels * static_cast<int>(sizeof(float));
    pw_stream_queue_buffer(state.stream, buffer);
}

constexpr pw_stream_events events{
    PW_VERSION_STREAM_EVENTS, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, onProcess, nullptr, nullptr, nullptr,
};

} // namespace

PipeWireVirtualMicrophone::PipeWireVirtualMicrophone(std::size_t ringBufferBytes)
    : state_(std::make_unique<State>()) {
    const auto aligned = std::max<std::size_t>(ringBufferBytes & ~std::size_t{sizeof(float) - 1U}, sizeof(float) * 2U);
    state_->storage.resize(aligned);
    spa_ringbuffer_init(&state_->ring);
}

PipeWireVirtualMicrophone::~PipeWireVirtualMicrophone() { stop(); }

bool PipeWireVirtualMicrophone::initialize() {
    if (state_->initialized) return true;
    pw_init(nullptr, nullptr);
    state_->loop = pw_thread_loop_new("puffy-virtual-microphone", nullptr);
    if (state_->loop == nullptr) return false;
    if (pw_thread_loop_start(state_->loop) < 0) { pw_thread_loop_destroy(state_->loop); state_->loop = nullptr; return false; }
    state_->initialized = true;
    return true;
}

bool PipeWireVirtualMicrophone::start() {
    if (!state_->initialized || state_->started) return false;
    state_->format = {48000, 2};
    pw_thread_loop_lock(state_->loop);
    auto* properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_MEDIA_CLASS, "Audio/Source",
        PW_KEY_NODE_NAME, "puffy_virtual_microphone",
        PW_KEY_NODE_DESCRIPTION, "OpenSoundboard Virtual Microphone",
        PW_KEY_NODE_LATENCY, "128/48000",
        PW_KEY_NODE_FORCE_QUANTUM, "128",
        nullptr);
    state_->stream = pw_stream_new_simple(pw_thread_loop_get_loop(state_->loop),
                                           "OpenSoundboard Virtual Microphone",
                                           properties, &events, state_.get());
    std::uint8_t podBuffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
    spa_audio_info_raw raw = SPA_AUDIO_INFO_RAW_INIT();
    raw.format = SPA_AUDIO_FORMAT_F32; raw.rate = 48000; raw.channels = 2;
    const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &raw)};
    const auto result = state_->stream == nullptr ? -1 : pw_stream_connect(
        state_->stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS), params, 1);
    pw_thread_loop_unlock(state_->loop);
    if (result < 0) { stop(); return false; }
    state_->started = true;
    return true;
}

void PipeWireVirtualMicrophone::stop() noexcept {
    if (state_->loop == nullptr) return;
    pw_thread_loop_lock(state_->loop);
    if (state_->stream != nullptr) { pw_stream_disconnect(state_->stream); pw_stream_destroy(state_->stream); state_->stream = nullptr; }
    pw_thread_loop_unlock(state_->loop);
    pw_thread_loop_stop(state_->loop);
    pw_thread_loop_destroy(state_->loop);
    state_->loop = nullptr; state_->started = false; state_->initialized = false;
}

bool PipeWireVirtualMicrophone::pushAudio(std::span<const float> samples, std::size_t frames,
                                          int channels, int sampleRate) noexcept {
    if (!state_->started || channels != state_->format.channels || sampleRate != state_->format.sampleRate) return false;
    const auto bytes = frames * static_cast<std::size_t>(channels) * sizeof(float);
    if (bytes == 0 || bytes > state_->storage.size()) return false;
    std::uint32_t write = 0;
    const auto used = spa_ringbuffer_get_write_index(&state_->ring, &write);
    const auto available = used > 0 ? static_cast<std::size_t>(used) : 0U;
    if (available > state_->storage.size() || state_->storage.size() - available < bytes) {
        // A virtual microphone must remain live. Discard old queued audio
        // rather than delivering it seconds late to Discord/OBS.
        const auto newRead = write + static_cast<std::uint32_t>(bytes)
            - static_cast<std::uint32_t>(state_->storage.size());
        spa_ringbuffer_read_update(&state_->ring, static_cast<std::int32_t>(newRead));
    }
    spa_ringbuffer_write_data(&state_->ring, state_->storage.data(), static_cast<std::uint32_t>(state_->storage.size()),
                              write % static_cast<std::uint32_t>(state_->storage.size()), samples.data(), static_cast<std::uint32_t>(bytes));
    spa_ringbuffer_write_update(&state_->ring, static_cast<std::int32_t>(write + bytes));
    return true;
}

} // namespace puffy::platform::linux
