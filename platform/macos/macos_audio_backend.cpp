#include "platform/macos/macos_audio_backend.hpp"

#include "core/audio/spsc_audio_ring.hpp"
#include "virtual_audio/virtual_device_contract.hpp"

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <utility>
#include <vector>

namespace puffy::platform::macos {
namespace {

std::string stringValue(CFStringRef value) {
    if (value == nullptr) return {};
    const auto length = CFStringGetLength(value);
    const auto capacity = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::vector<char> buffer(static_cast<std::size_t>(capacity));
    return CFStringGetCString(value, buffer.data(), capacity, kCFStringEncodingUTF8)
        ? std::string(buffer.data()) : std::string{};
}

AudioDeviceID defaultDevice(bool input) {
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress address{
        input ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &device);
    return device;
}

UInt32 channelCount(AudioDeviceID device, bool input) {
    AudioObjectPropertyAddress address{kAudioDevicePropertyStreamConfiguration,
        input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &address, 0, nullptr, &size) != noErr || size == 0) return 0;
    std::vector<std::byte> storage(size);
    auto* buffers = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, buffers) != noErr) return 0;
    UInt32 channels = 0;
    for (UInt32 index = 0; index < buffers->mNumberBuffers; ++index) channels += buffers->mBuffers[index].mNumberChannels;
    return channels;
}

std::string deviceString(AudioDeviceID device, AudioObjectPropertySelector selector) {
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    AudioObjectPropertyAddress address{selector, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &value) != noErr || value == nullptr) return {};
    auto result = stringValue(value);
    CFRelease(value);
    return result;
}

double deviceSampleRate(AudioDeviceID device) {
    Float64 rate = 48'000.0;
    UInt32 size = sizeof(rate);
    AudioObjectPropertyAddress address{kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &rate);
    return rate;
}

std::vector<audio::AudioDeviceInfo> enumerate(bool input) {
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr) return {};
    std::vector<AudioDeviceID> ids(size / sizeof(AudioDeviceID));
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, ids.data()) != noErr) return {};
    const auto systemDefault = defaultDevice(input);
    std::vector<audio::AudioDeviceInfo> result;
    for (const auto id : ids) {
        const auto channels = channelCount(id, input);
        if (channels == 0) continue;
        auto uid = deviceString(id, kAudioDevicePropertyDeviceUID);
        auto name = deviceString(id, kAudioObjectPropertyName);
        if (uid.empty()) continue;
        result.push_back({std::move(uid), name.empty() ? "Core Audio device" : std::move(name),
                          {static_cast<int>(deviceSampleRate(id)), static_cast<int>(channels)},
                          id == systemDefault});
    }
    return result;
}

AudioDeviceID resolveDevice(std::string_view uid, bool input) {
    if (uid.empty() || uid == "default") return defaultDevice(input);
    const auto devices = enumerate(input);
    const auto found = std::find_if(devices.begin(), devices.end(), [uid](const auto& device) { return device.id == uid; });
    if (found == devices.end()) return kAudioObjectUnknown;
    CFStringRef requested = CFStringCreateWithCString(kCFAllocatorDefault, found->id.c_str(), kCFStringEncodingUTF8);
    AudioDeviceID result = kAudioObjectUnknown;
    UInt32 size = sizeof(AudioValueTranslation);
    AudioValueTranslation translation{&requested, sizeof(requested), &result, sizeof(result)};
    AudioObjectPropertyAddress address{kAudioHardwarePropertyDeviceForUID,
        kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &translation);
    if (requested) CFRelease(requested);
    return result;
}

AudioUnit createAuHal() {
    AudioComponentDescription description{kAudioUnitType_Output, kAudioUnitSubType_HALOutput,
                                           kAudioUnitManufacturer_Apple, 0, 0};
    const auto component = AudioComponentFindNext(nullptr, &description);
    AudioUnit unit = nullptr;
    if (component == nullptr || AudioComponentInstanceNew(component, &unit) != noErr) return nullptr;
    return unit;
}

AudioStreamBasicDescription floatFormat(audio::AudioFormat format) {
    AudioStreamBasicDescription stream{};
    stream.mSampleRate = static_cast<Float64>(format.sampleRate);
    stream.mFormatID = kAudioFormatLinearPCM;
    stream.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    stream.mBytesPerPacket = static_cast<UInt32>(format.channels * sizeof(float));
    stream.mFramesPerPacket = 1;
    stream.mBytesPerFrame = stream.mBytesPerPacket;
    stream.mChannelsPerFrame = static_cast<UInt32>(format.channels);
    stream.mBitsPerChannel = 32;
    return stream;
}

} // namespace

struct CoreAudioCapture::State {
    AudioUnit unit{nullptr};
    AudioDeviceID device{kAudioObjectUnknown};
    audio::AudioFormat format{};
    Callback callback;
    std::vector<float> scratch;
    std::atomic<bool> open{false};
};

namespace {
OSStatus captureCallback(void* userData, AudioUnitRenderActionFlags* flags,
                         const AudioTimeStamp* timestamp, UInt32, UInt32 frames, AudioBufferList*) {
    auto& state = *static_cast<CoreAudioCapture::State*>(userData);
    const auto samples = static_cast<std::size_t>(frames) * static_cast<std::size_t>(state.format.channels);
    if (!state.open || samples > state.scratch.size()) return noErr;
    AudioBufferList list{};
    list.mNumberBuffers = 1;
    list.mBuffers[0].mNumberChannels = static_cast<UInt32>(state.format.channels);
    list.mBuffers[0].mDataByteSize = static_cast<UInt32>(samples * sizeof(float));
    list.mBuffers[0].mData = state.scratch.data();
    const auto status = AudioUnitRender(state.unit, flags, timestamp, 1, frames, &list);
    if (status == noErr && state.callback)
        state.callback({std::span<const float>(state.scratch.data(), samples), frames, state.format.channels});
    return status;
}
} // namespace

CoreAudioCapture::CoreAudioCapture() : state_(std::make_unique<State>()) {}
CoreAudioCapture::~CoreAudioCapture() { close(); }
std::vector<audio::AudioDeviceInfo> CoreAudioCapture::devices() const { return enumerate(true); }

bool CoreAudioCapture::open(const std::string& deviceId, audio::AudioFormat format, Callback callback) {
    if (state_->open || format.channels <= 0 || format.sampleRate <= 0 || !callback) return false;
    state_->device = resolveDevice(deviceId, true);
    state_->unit = createAuHal();
    if (state_->device == kAudioObjectUnknown || state_->unit == nullptr) { close(); return false; }
    UInt32 enabled = 1;
    UInt32 disabled = 0;
    UInt32 maxFrames = 4096;
    auto stream = floatFormat(format);
    AURenderCallbackStruct handler{captureCallback, state_.get()};
    const bool configured =
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1,
                             &enabled, sizeof(enabled)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0,
                             &disabled, sizeof(disabled)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0,
                             &state_->device, sizeof(state_->device)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1,
                             &stream, sizeof(stream)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
                             &maxFrames, sizeof(maxFrames)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0,
                             &handler, sizeof(handler)) == noErr;
    if (!configured) { close(); return false; }
    state_->format = format;
    state_->callback = std::move(callback);
    state_->scratch.resize(static_cast<std::size_t>(maxFrames) * static_cast<std::size_t>(format.channels));
    if (AudioUnitInitialize(state_->unit) != noErr) { close(); return false; }
    state_->open = true;
    if (AudioOutputUnitStart(state_->unit) != noErr) { close(); return false; }
    return true;
}

void CoreAudioCapture::close() noexcept {
    state_->open = false;
    if (state_->unit != nullptr) {
        AudioOutputUnitStop(state_->unit);
        AudioUnitUninitialize(state_->unit);
        AudioComponentInstanceDispose(state_->unit);
    }
    state_->unit = nullptr;
    state_->device = kAudioObjectUnknown;
    state_->callback = {};
    state_->scratch.clear();
}

bool CoreAudioCapture::isOpen() const noexcept { return state_->open; }

struct CoreAudioOutput::State {
    explicit State(std::size_t samples) : ring(samples) {}
    audio::SpscAudioRing ring;
    AudioUnit unit{nullptr};
    AudioDeviceID device{kAudioObjectUnknown};
    audio::AudioFormat format{};
    std::atomic<bool> open{false};
    std::atomic<bool> started{false};
};

namespace {
OSStatus outputCallback(void* userData, AudioUnitRenderActionFlags*, const AudioTimeStamp*,
                        UInt32, UInt32 frames, AudioBufferList* data) {
    auto& state = *static_cast<CoreAudioOutput::State*>(userData);
    if (data == nullptr) return noErr;
    for (UInt32 index = 0; index < data->mNumberBuffers; ++index)
        if (data->mBuffers[index].mData != nullptr)
            std::memset(data->mBuffers[index].mData, 0, data->mBuffers[index].mDataByteSize);
    if (!state.started || data->mNumberBuffers != 1 || data->mBuffers[0].mData == nullptr) return noErr;
    const auto requested = static_cast<std::size_t>(frames) * static_cast<std::size_t>(state.format.channels);
    const auto capacity = static_cast<std::size_t>(data->mBuffers[0].mDataByteSize) / sizeof(float);
    state.ring.read(std::span<float>(static_cast<float*>(data->mBuffers[0].mData), std::min(requested, capacity)));
    return noErr;
}
} // namespace

CoreAudioOutput::CoreAudioOutput(std::size_t ringBufferSamples)
    : state_(std::make_unique<State>(ringBufferSamples)) {}
CoreAudioOutput::~CoreAudioOutput() { stop(); }
std::vector<audio::AudioDeviceInfo> CoreAudioOutput::devices() const { return enumerate(false); }

bool CoreAudioOutput::open(const std::string& deviceId, audio::AudioFormat format) {
    if (state_->open || format.channels <= 0 || format.sampleRate <= 0) return false;
    state_->device = resolveDevice(deviceId, false);
    state_->unit = createAuHal();
    if (state_->device == kAudioObjectUnknown || state_->unit == nullptr) { stop(); return false; }
    UInt32 enabled = 1;
    UInt32 disabled = 0;
    UInt32 maxFrames = 4096;
    auto stream = floatFormat(format);
    AURenderCallbackStruct handler{outputCallback, state_.get()};
    const bool configured =
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0,
                             &enabled, sizeof(enabled)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1,
                             &disabled, sizeof(disabled)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0,
                             &state_->device, sizeof(state_->device)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                             &stream, sizeof(stream)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
                             &maxFrames, sizeof(maxFrames)) == noErr &&
        AudioUnitSetProperty(state_->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &handler, sizeof(handler)) == noErr;
    if (!configured || AudioUnitInitialize(state_->unit) != noErr) { stop(); return false; }
    state_->format = format;
    state_->ring.clear();
    state_->open = true;
    return true;
}

bool CoreAudioOutput::start() {
    if (!state_->open || state_->started) return false;
    state_->started = AudioOutputUnitStart(state_->unit) == noErr;
    return state_->started;
}

void CoreAudioOutput::stop() noexcept {
    state_->started = false;
    if (state_->unit != nullptr) {
        AudioOutputUnitStop(state_->unit);
        AudioUnitUninitialize(state_->unit);
        AudioComponentInstanceDispose(state_->unit);
    }
    state_->unit = nullptr;
    state_->device = kAudioObjectUnknown;
    state_->open = false;
    state_->ring.clear();
}

bool CoreAudioOutput::write(std::span<const float> samples, std::size_t frames) noexcept {
    if (!state_->started || state_->format.channels <= 0) return false;
    const auto count = frames * static_cast<std::size_t>(state_->format.channels);
    return samples.size() >= count && state_->ring.write(samples.first(count));
}

bool CoreAudioOutput::isOpen() const noexcept { return state_->open && state_->started; }

MacOSVirtualMicrophone::MacOSVirtualMicrophone(std::string transportDeviceUid)
    : deviceUid_(std::move(transportDeviceUid)), output_(std::make_unique<CoreAudioOutput>()) {}
MacOSVirtualMicrophone::~MacOSVirtualMicrophone() { stop(); }
void MacOSVirtualMicrophone::setTransportDeviceUid(std::string deviceUid) {
    if (!initialized_) deviceUid_ = std::move(deviceUid);
}
bool MacOSVirtualMicrophone::initialize() {
    if (deviceUid_.empty()) {
        const auto devices = output_->devices();
        const auto found = std::find_if(devices.begin(), devices.end(), [](const auto& device) {
            return device.id == virtual_audio::macosTransportDeviceUid ||
                   device.name == virtual_audio::macosTransportDeviceName;
        });
        if (found != devices.end()) deviceUid_ = found->id;
    }
    if (deviceUid_.empty()) return false;
    initialized_ = output_->open(deviceUid_, format_);
    return initialized_;
}
bool MacOSVirtualMicrophone::start() { return initialized_ && output_->start(); }
void MacOSVirtualMicrophone::stop() noexcept {
    if (output_) output_->stop();
    initialized_ = false;
}
bool MacOSVirtualMicrophone::pushAudio(std::span<const float> samples, std::size_t frames,
                                       int channels, int sampleRate) noexcept {
    if (!initialized_ || channels != format_.channels || sampleRate != format_.sampleRate) return false;
    return output_->write(samples, frames);
}

} // namespace puffy::platform::macos
