#include "platform/macos/macos_audio_backend.hpp"

namespace puffy::platform::macos {

std::vector<audio::AudioDeviceInfo> CoreAudioCapture::devices() const { return {}; }
bool CoreAudioCapture::open(const std::string&, audio::AudioFormat, Callback) { return false; }
void CoreAudioCapture::close() noexcept {}

std::vector<audio::AudioDeviceInfo> CoreAudioOutput::devices() const { return {}; }
bool CoreAudioOutput::open(const std::string&, audio::AudioFormat) { return false; }
bool CoreAudioOutput::start() { return false; }
void CoreAudioOutput::stop() noexcept {}
bool CoreAudioOutput::write(std::span<const float>, std::size_t) noexcept { return false; }

bool MacOSVirtualMicrophone::initialize() { return false; }
bool MacOSVirtualMicrophone::start() { return false; }
void MacOSVirtualMicrophone::stop() noexcept {}
bool MacOSVirtualMicrophone::pushAudio(std::span<const float>, std::size_t, int, int) noexcept { return false; }

} // namespace puffy::platform::macos
