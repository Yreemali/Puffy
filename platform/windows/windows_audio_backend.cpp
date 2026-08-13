#include "platform/windows/windows_audio_backend.hpp"

namespace puffy::platform::windows {

std::vector<audio::AudioDeviceInfo> WasapiCapture::devices() const { return {}; }
bool WasapiCapture::open(const std::string&, audio::AudioFormat, Callback) { return false; }
void WasapiCapture::close() noexcept {}

std::vector<audio::AudioDeviceInfo> WasapiOutput::devices() const { return {}; }
bool WasapiOutput::open(const std::string&, audio::AudioFormat) { return false; }
bool WasapiOutput::start() { return false; }
void WasapiOutput::stop() noexcept {}
bool WasapiOutput::write(std::span<const float>, std::size_t) noexcept { return false; }

bool WindowsVirtualMicrophone::initialize() { return false; }
bool WindowsVirtualMicrophone::start() { return false; }
void WindowsVirtualMicrophone::stop() noexcept {}
bool WindowsVirtualMicrophone::pushAudio(std::span<const float>, std::size_t, int, int) noexcept { return false; }

} // namespace puffy::platform::windows
