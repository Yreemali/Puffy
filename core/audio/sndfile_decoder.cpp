#include "core/audio/audio_decoder.hpp"

#ifdef PUFFY_HAS_SNDFILE
#include <sndfile.h>
#endif

namespace puffy::audio {

std::shared_ptr<const DecodedAudio> SndFileDecoder::decode(const std::filesystem::path& path, std::string& error) const {
#ifdef PUFFY_HAS_SNDFILE
    SF_INFO info{};
    SNDFILE* file = sf_open(path.string().c_str(), SFM_READ, &info);
    if (file == nullptr) { error = sf_strerror(nullptr); return {}; }
    if (info.channels <= 0 || info.samplerate <= 0 || info.frames < 0) { sf_close(file); error = "Invalid audio metadata"; return {}; }
    auto decoded = std::make_shared<DecodedAudio>();
    decoded->sampleRate = info.samplerate;
    decoded->channels = info.channels;
    decoded->samples.resize(static_cast<std::size_t>(info.frames) * static_cast<std::size_t>(info.channels));
    const auto framesRead = sf_readf_float(file, decoded->samples.data(), info.frames);
    sf_close(file);
    if (framesRead != info.frames) { error = "Could not decode complete audio file"; return {}; }
    return decoded;
#else
    (void)path;
    error = "libsndfile support is not available in this build";
    return {};
#endif
}

} // namespace puffy::audio
