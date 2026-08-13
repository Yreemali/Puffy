#include "platform/windows/windows_audio_backend.hpp"

#include "core/audio/spsc_audio_ring.hpp"
#include "virtual_audio/virtual_device_contract.hpp"

#define NOMINMAX
#include <Windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace puffy::platform::windows {
namespace {

template <typename T> void release(T*& value) noexcept {
    if (value != nullptr) value->Release();
    value = nullptr;
}

class ComScope final {
public:
    ComScope() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ComScope() { if (SUCCEEDED(result_)) CoUninitialize(); }
    [[nodiscard]] bool ready() const noexcept {
        return SUCCEEDED(result_) || result_ == RPC_E_CHANGED_MODE;
    }
private:
    HRESULT result_;
};

std::string utf8(const wchar_t* value) {
    if (value == nullptr) return {};
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(size - 1));
    return result;
}

std::wstring wide(std::string_view value) {
    if (value.empty()) return {};
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(std::max(0, size)), L'\0');
    if (size > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

WAVEFORMATEXTENSIBLE makeFloatFormat(audio::AudioFormat format) {
    WAVEFORMATEXTENSIBLE wave{};
    wave.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wave.Format.nChannels = static_cast<WORD>(format.channels);
    wave.Format.nSamplesPerSec = static_cast<DWORD>(format.sampleRate);
    wave.Format.wBitsPerSample = 32;
    wave.Format.nBlockAlign = static_cast<WORD>(format.channels * sizeof(float));
    wave.Format.nAvgBytesPerSec = wave.Format.nSamplesPerSec * wave.Format.nBlockAlign;
    wave.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wave.Samples.wValidBitsPerSample = 32;
    wave.dwChannelMask = format.channels == 1 ? SPEAKER_FRONT_CENTER
                                              : format.channels == 2 ? SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT : 0;
    wave.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    return wave;
}

IMMDevice* endpoint(EDataFlow flow, const std::string& id) {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))) return nullptr;
    if (id.empty() || id == "default") {
        enumerator->GetDefaultAudioEndpoint(flow, eCommunications, &device);
        if (device == nullptr) enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device);
    } else {
        const auto deviceId = wide(id);
        enumerator->GetDevice(deviceId.c_str(), &device);
    }
    release(enumerator);
    return device;
}

std::vector<audio::AudioDeviceInfo> enumerate(EDataFlow flow) {
    ComScope com;
    if (!com.ready()) return {};
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDeviceCollection* collection = nullptr;
    IMMDevice* defaultDevice = nullptr;
    LPWSTR defaultId = nullptr;
    std::vector<audio::AudioDeviceInfo> result;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))) return result;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eCommunications, &defaultDevice)) && defaultDevice != nullptr)
        defaultDevice->GetId(&defaultId);
    if (FAILED(enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection))) {
        if (defaultId) CoTaskMemFree(defaultId);
        release(defaultDevice); release(enumerator); return result;
    }
    UINT count = 0;
    collection->GetCount(&count);
    result.reserve(count);
    for (UINT index = 0; index < count; ++index) {
        IMMDevice* device = nullptr;
        IPropertyStore* properties = nullptr;
        IAudioClient* client = nullptr;
        LPWSTR id = nullptr;
        PROPVARIANT name;
        PropVariantInit(&name);
        if (FAILED(collection->Item(index, &device)) || device == nullptr) continue;
        device->GetId(&id);
        device->OpenPropertyStore(STGM_READ, &properties);
        if (properties) properties->GetValue(PKEY_Device_FriendlyName, &name);
        audio::AudioFormat preferred{48'000, flow == eCapture ? 1 : 2};
        if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void**>(&client))) && client != nullptr) {
            WAVEFORMATEX* mix = nullptr;
            if (SUCCEEDED(client->GetMixFormat(&mix)) && mix != nullptr) {
                preferred.sampleRate = static_cast<int>(mix->nSamplesPerSec);
                preferred.channels = static_cast<int>(mix->nChannels);
                CoTaskMemFree(mix);
            }
        }
        const auto endpointId = utf8(id);
        result.push_back({endpointId,
                          name.vt == VT_LPWSTR ? utf8(name.pwszVal) : endpointId,
                          preferred, id != nullptr && defaultId != nullptr && wcscmp(id, defaultId) == 0});
        PropVariantClear(&name);
        if (id) CoTaskMemFree(id);
        release(client); release(properties); release(device);
    }
    if (defaultId) CoTaskMemFree(defaultId);
    release(defaultDevice); release(collection); release(enumerator);
    return result;
}

void signalReady(std::mutex& mutex, std::condition_variable& condition,
                 bool& finished, bool& success, bool value) {
    {
        std::lock_guard lock(mutex);
        success = value;
        finished = true;
    }
    condition.notify_one();
}

constexpr DWORD streamFlags = static_cast<DWORD>(AUDCLNT_STREAMFLAGS_EVENTCALLBACK) |
                              static_cast<DWORD>(AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM) |
                              static_cast<DWORD>(AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY);

} // namespace

struct WasapiCapture::State {
    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> open{false};
    HANDLE stopEvent{nullptr};
    audio::AudioFormat format{};
    std::string deviceId;
    Callback callback;
    std::mutex readyMutex;
    std::condition_variable readyCondition;
    bool initializationFinished{false};
    bool initializationSucceeded{false};
};

WasapiCapture::WasapiCapture() : state_(std::make_unique<State>()) {}
WasapiCapture::~WasapiCapture() { close(); }

std::vector<audio::AudioDeviceInfo> WasapiCapture::devices() const { return enumerate(eCapture); }

bool WasapiCapture::open(const std::string& deviceId, audio::AudioFormat format, Callback callback) {
    if (state_->open || state_->thread.joinable() || format.channels <= 0 || format.sampleRate <= 0 || !callback) return false;
    state_->deviceId = deviceId;
    state_->format = format;
    state_->callback = std::move(callback);
    state_->initializationFinished = false;
    state_->initializationSucceeded = false;
    state_->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (state_->stopEvent == nullptr) return false;
    state_->running = true;
    state_->thread = std::thread([state = state_.get()] {
        ComScope com;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        IAudioCaptureClient* capture = nullptr;
        HANDLE audioEvent = nullptr;
        HANDLE mmcss = nullptr;
        DWORD taskIndex = 0;
        UINT32 endpointFrames = 0;
        std::vector<float> silence;
        bool ready = false;
        if (com.ready()) device = endpoint(eCapture, state->deviceId);
        if (device != nullptr && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                                            reinterpret_cast<void**>(&client)))) {
            auto wave = makeFloatFormat(state->format);
            if (SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, 0, 0,
                                             &wave.Format, nullptr))) {
                audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (audioEvent != nullptr && SUCCEEDED(client->SetEventHandle(audioEvent)) &&
                    SUCCEEDED(client->GetBufferSize(&endpointFrames)) &&
                    SUCCEEDED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture))) &&
                    SUCCEEDED(client->Start())) {
                    silence.resize(static_cast<std::size_t>(endpointFrames) * static_cast<std::size_t>(state->format.channels), 0.0F);
                    ready = true;
                }
            }
        }
        state->open = ready;
        signalReady(state->readyMutex, state->readyCondition, state->initializationFinished,
                    state->initializationSucceeded, ready);
        if (ready) {
            mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
            HANDLE waits[]{state->stopEvent, audioEvent};
            while (state->running) {
                const auto wait = WaitForMultipleObjects(2, waits, FALSE, 500);
                if (wait == WAIT_OBJECT_0) break;
                if (wait != WAIT_OBJECT_0 + 1) continue;
                UINT32 packetFrames = 0;
                while (SUCCEEDED(capture->GetNextPacketSize(&packetFrames)) && packetFrames > 0) {
                    BYTE* bytes = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    if (FAILED(capture->GetBuffer(&bytes, &frames, &flags, nullptr, nullptr))) break;
                    const auto samples = static_cast<std::size_t>(frames) * static_cast<std::size_t>(state->format.channels);
                    if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0) {
                        if (samples <= silence.size())
                            state->callback({std::span<const float>(silence.data(), samples), frames, state->format.channels});
                    } else if (bytes != nullptr) {
                        state->callback({std::span<const float>(reinterpret_cast<const float*>(bytes), samples),
                                         frames, state->format.channels});
                    }
                    capture->ReleaseBuffer(frames);
                }
            }
        }
        if (client) client->Stop();
        if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
        if (audioEvent) CloseHandle(audioEvent);
        release(capture); release(client); release(device);
        state->open = false;
    });
    std::unique_lock lock(state_->readyMutex);
    state_->readyCondition.wait(lock, [this] { return state_->initializationFinished; });
    if (!state_->initializationSucceeded) { lock.unlock(); close(); return false; }
    return true;
}

void WasapiCapture::close() noexcept {
    state_->running = false;
    if (state_->stopEvent) SetEvent(state_->stopEvent);
    if (state_->thread.joinable()) state_->thread.join();
    if (state_->stopEvent) CloseHandle(state_->stopEvent);
    state_->stopEvent = nullptr;
    state_->open = false;
    state_->callback = {};
}

bool WasapiCapture::isOpen() const noexcept { return state_->open; }

struct WasapiOutput::State {
    explicit State(std::size_t samples) : ring(samples) {}
    audio::SpscAudioRing ring;
    std::thread thread;
    std::atomic<bool> running{false};
    std::atomic<bool> open{false};
    std::atomic<bool> started{false};
    HANDLE stopEvent{nullptr};
    audio::AudioFormat format{};
    std::string deviceId;
    std::mutex readyMutex;
    std::condition_variable readyCondition;
    bool initializationFinished{false};
    bool initializationSucceeded{false};
};

WasapiOutput::WasapiOutput(std::size_t ringBufferSamples)
    : state_(std::make_unique<State>(ringBufferSamples)) {}
WasapiOutput::~WasapiOutput() { stop(); }

std::vector<audio::AudioDeviceInfo> WasapiOutput::devices() const { return enumerate(eRender); }

bool WasapiOutput::open(const std::string& deviceId, audio::AudioFormat format) {
    if (state_->open || state_->thread.joinable() || format.channels <= 0 || format.sampleRate <= 0) return false;
    state_->deviceId = deviceId;
    state_->format = format;
    state_->ring.clear();
    state_->open = true;
    return true;
}

bool WasapiOutput::start() {
    if (!state_->open || state_->started || state_->thread.joinable()) return false;
    state_->stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (state_->stopEvent == nullptr) return false;
    state_->running = true;
    state_->initializationFinished = false;
    state_->initializationSucceeded = false;
    state_->thread = std::thread([state = state_.get()] {
        ComScope com;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        IAudioRenderClient* render = nullptr;
        HANDLE audioEvent = nullptr;
        HANDLE mmcss = nullptr;
        DWORD taskIndex = 0;
        UINT32 bufferFrames = 0;
        bool ready = false;
        if (com.ready()) device = endpoint(eRender, state->deviceId);
        if (device != nullptr && SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                                            reinterpret_cast<void**>(&client)))) {
            auto wave = makeFloatFormat(state->format);
            if (SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, 0, 0,
                                             &wave.Format, nullptr)) &&
                SUCCEEDED(client->GetBufferSize(&bufferFrames))) {
                audioEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
                if (audioEvent != nullptr && SUCCEEDED(client->SetEventHandle(audioEvent)) &&
                    SUCCEEDED(client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render)))) {
                    BYTE* initial = nullptr;
                    if (SUCCEEDED(render->GetBuffer(bufferFrames, &initial))) {
                        render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
                        ready = SUCCEEDED(client->Start());
                    }
                }
            }
        }
        state->started = ready;
        signalReady(state->readyMutex, state->readyCondition, state->initializationFinished,
                    state->initializationSucceeded, ready);
        if (ready) {
            mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
            HANDLE waits[]{state->stopEvent, audioEvent};
            while (state->running) {
                const auto wait = WaitForMultipleObjects(2, waits, FALSE, 500);
                if (wait == WAIT_OBJECT_0) break;
                if (wait != WAIT_OBJECT_0 + 1) continue;
                UINT32 padding = 0;
                if (FAILED(client->GetCurrentPadding(&padding)) || padding >= bufferFrames) continue;
                const auto frames = bufferFrames - padding;
                BYTE* bytes = nullptr;
                if (FAILED(render->GetBuffer(frames, &bytes)) || bytes == nullptr) continue;
                const auto sampleCount = static_cast<std::size_t>(frames) * static_cast<std::size_t>(state->format.channels);
                auto destination = std::span<float>(reinterpret_cast<float*>(bytes), sampleCount);
                const auto read = state->ring.read(destination);
                std::fill(destination.begin() + static_cast<std::ptrdiff_t>(read), destination.end(), 0.0F);
                render->ReleaseBuffer(frames, read == 0 ? static_cast<DWORD>(AUDCLNT_BUFFERFLAGS_SILENT) : 0U);
            }
        }
        if (client) client->Stop();
        if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
        if (audioEvent) CloseHandle(audioEvent);
        release(render); release(client); release(device);
        state->started = false;
        state->open = false;
    });
    std::unique_lock lock(state_->readyMutex);
    state_->readyCondition.wait(lock, [this] { return state_->initializationFinished; });
    if (!state_->initializationSucceeded) { lock.unlock(); stop(); return false; }
    return true;
}

void WasapiOutput::stop() noexcept {
    state_->running = false;
    if (state_->stopEvent) SetEvent(state_->stopEvent);
    if (state_->thread.joinable()) state_->thread.join();
    if (state_->stopEvent) CloseHandle(state_->stopEvent);
    state_->stopEvent = nullptr;
    state_->ring.clear();
    state_->started = false;
    state_->open = false;
}

bool WasapiOutput::write(std::span<const float> samples, std::size_t frames) noexcept {
    if (!state_->started || state_->format.channels <= 0) return false;
    const auto count = frames * static_cast<std::size_t>(state_->format.channels);
    return samples.size() >= count && state_->ring.write(samples.first(count));
}

bool WasapiOutput::isOpen() const noexcept { return state_->open && state_->started; }

WindowsVirtualMicrophone::WindowsVirtualMicrophone(std::string transportEndpointId)
    : endpointId_(std::move(transportEndpointId)), output_(std::make_unique<WasapiOutput>()) {}
WindowsVirtualMicrophone::~WindowsVirtualMicrophone() { stop(); }

void WindowsVirtualMicrophone::setTransportEndpointId(std::string endpointId) {
    if (!initialized_) endpointId_ = std::move(endpointId);
}

bool WindowsVirtualMicrophone::initialize() {
    if (endpointId_.empty()) {
        const auto endpoints = output_->devices();
        const auto found = std::find_if(endpoints.begin(), endpoints.end(), [](const auto& device) {
            return device.name == virtual_audio::windowsTransportFriendlyName;
        });
        if (found != endpoints.end()) endpointId_ = found->id;
    }
    if (endpointId_.empty()) return false;
    initialized_ = output_->open(endpointId_, format_);
    return initialized_;
}

bool WindowsVirtualMicrophone::start() { return initialized_ && output_->start(); }

void WindowsVirtualMicrophone::stop() noexcept {
    if (output_) output_->stop();
    initialized_ = false;
}

bool WindowsVirtualMicrophone::pushAudio(std::span<const float> samples, std::size_t frames,
                                         int channels, int sampleRate) noexcept {
    if (!initialized_ || channels != format_.channels || sampleRate != format_.sampleRate) return false;
    return output_->write(samples, frames);
}

} // namespace puffy::platform::windows
