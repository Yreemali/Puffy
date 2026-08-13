#pragma once

#include <string_view>

namespace puffy::virtual_audio {

inline constexpr std::string_view publicMicrophoneName = "Puffy Virtual Microphone";

// The app writes mixed float32 audio to this render endpoint. The separately
// installed platform component exposes the same stream as a capture endpoint.
inline constexpr std::string_view windowsTransportFriendlyName = "Puffy Virtual Microphone Transport";
inline constexpr std::string_view macosTransportDeviceUid = "dev.puffy.virtual-audio.transport";
inline constexpr std::string_view macosTransportDeviceName = "Puffy Virtual Microphone Transport";

inline constexpr int transportSampleRate = 48'000;
inline constexpr int transportChannels = 2;

} // namespace puffy::virtual_audio
