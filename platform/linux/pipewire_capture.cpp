#include "platform/linux/pipewire_capture.hpp"

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

#include <cstdint>

namespace puffy::platform::linux {

struct PipeWireCapture::State {
    pw_thread_loop* threadLoop{nullptr};
    pw_stream* stream{nullptr};
    audio::AudioFormat format{};
    Callback callback;
    bool open{false};
};

namespace {

void onProcess(void* userData) {
    auto& state = *static_cast<PipeWireCapture::State*>(userData);
    auto* buffer = pw_stream_dequeue_buffer(state.stream);
    if (buffer == nullptr || buffer->buffer->n_datas == 0) return;
    auto& data = buffer->buffer->datas[0];
    if (data.data != nullptr && data.chunk != nullptr && state.callback != nullptr) {
        const auto byteCount = static_cast<std::size_t>(data.chunk->size);
        const auto sampleCount = byteCount / sizeof(float);
        const auto frames = sampleCount / static_cast<std::size_t>(state.format.channels);
        state.callback(audio::AudioBlock{
            std::span<const float>(static_cast<const float*>(data.data), sampleCount),
            frames,
            state.format.channels,
        });
    }
    pw_stream_queue_buffer(state.stream, buffer);
}

constexpr pw_stream_events events{
    PW_VERSION_STREAM_EVENTS, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, onProcess, nullptr, nullptr, nullptr,
};

} // namespace

PipeWireCapture::~PipeWireCapture() { close(); }

PipeWireCapture::PipeWireCapture() = default;

std::vector<audio::AudioDeviceInfo> PipeWireCapture::devices() const {
    // The session manager resolves the default source. Full enumeration will use
    // a pw_registry listener in the device-management module.
    return {{"default", "PipeWire default microphone", {48000, 1}, true}};
}

bool PipeWireCapture::open(const std::string& deviceId, audio::AudioFormat format, Callback callback) {
    if (state_ != nullptr || format.channels <= 0 || format.sampleRate <= 0 || !callback) return false;
    state_ = std::make_unique<State>();
    state_->format = format;
    state_->callback = std::move(callback);
    pw_init(nullptr, nullptr);
    state_->threadLoop = pw_thread_loop_new("puffy-pipewire-capture", nullptr);
    if (state_->threadLoop == nullptr) { state_.reset(); return false; }
    if (pw_thread_loop_start(state_->threadLoop) < 0) { close(); return false; }

    pw_thread_loop_lock(state_->threadLoop);
    auto* properties = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                         PW_KEY_MEDIA_CATEGORY, "Capture",
                                         PW_KEY_MEDIA_ROLE, "Communication", nullptr);
    if (!deviceId.empty() && deviceId != "default") pw_properties_set(properties, PW_KEY_TARGET_OBJECT, deviceId.c_str());
    state_->stream = pw_stream_new_simple(pw_thread_loop_get_loop(state_->threadLoop), "puffy-microphone-capture", properties, &events, state_.get());
    std::uint8_t buffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    spa_audio_info_raw raw = SPA_AUDIO_INFO_RAW_INIT();
    raw.format = SPA_AUDIO_FORMAT_F32;
    raw.rate = static_cast<uint32_t>(format.sampleRate);
    raw.channels = static_cast<uint32_t>(format.channels);
    const spa_pod* params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &raw)};
    const auto result = state_->stream == nullptr ? -1 : pw_stream_connect(state_->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS), params, 1);
    pw_thread_loop_unlock(state_->threadLoop);
    if (result < 0) { close(); return false; }
    state_->open = true;
    return true;
}

void PipeWireCapture::close() noexcept {
    if (state_ == nullptr || state_->threadLoop == nullptr) { state_.reset(); return; }
    pw_thread_loop_lock(state_->threadLoop);
    if (state_->stream != nullptr) { pw_stream_disconnect(state_->stream); pw_stream_destroy(state_->stream); state_->stream = nullptr; }
    pw_thread_loop_unlock(state_->threadLoop);
    pw_thread_loop_stop(state_->threadLoop);
    pw_thread_loop_destroy(state_->threadLoop);
    state_.reset();
}

bool PipeWireCapture::isOpen() const noexcept { return state_ != nullptr && state_->open && state_->stream != nullptr; }

} // namespace puffy::platform::linux
