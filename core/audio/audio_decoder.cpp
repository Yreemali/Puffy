#include "core/audio/audio_decoder.hpp"

#include <bit>
#include <cstring>
#include <fstream>

namespace puffy::audio {
namespace {

std::uint16_t readU16(const std::vector<std::byte>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<unsigned char>(bytes[offset]) |
                                      (std::to_integer<unsigned char>(bytes[offset + 1]) << 8U));
}

std::uint32_t readU32(const std::vector<std::byte>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset]) |
                                      (std::to_integer<unsigned char>(bytes[offset + 1]) << 8U) |
                                      (std::to_integer<unsigned char>(bytes[offset + 2]) << 16U) |
                                      (std::to_integer<unsigned char>(bytes[offset + 3]) << 24U));
}

bool tagEquals(const std::vector<std::byte>& bytes, std::size_t offset, const char* tag) {
    return std::memcmp(bytes.data() + offset, tag, 4) == 0;
}

} // namespace

std::shared_ptr<const DecodedAudio> WavDecoder::decode(const std::filesystem::path& path, std::string& error) const {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { error = "Cannot open WAV file: " + path.string(); return {}; }
    const auto size = input.tellg();
    if (size < 44) { error = "WAV file is too small"; return {}; }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input || !tagEquals(bytes, 0, "RIFF") || !tagEquals(bytes, 8, "WAVE")) {
        error = "Unsupported WAV container"; return {};
    }

    std::size_t cursor = 12;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint16_t bitsPerSample = 0;
    while (cursor + 8 <= bytes.size()) {
        const auto chunkSize = static_cast<std::size_t>(readU32(bytes, cursor + 4));
        const auto chunkData = cursor + 8;
        if (chunkData > bytes.size() || chunkSize > bytes.size() - chunkData) { error = "Invalid WAV chunk"; return {}; }
        if (tagEquals(bytes, cursor, "fmt ") && chunkSize >= 16) {
            format = readU16(bytes, chunkData);
            channels = readU16(bytes, chunkData + 2);
            sampleRate = readU32(bytes, chunkData + 4);
            bitsPerSample = readU16(bytes, chunkData + 14);
        } else if (tagEquals(bytes, cursor, "data")) {
            dataOffset = chunkData; dataSize = chunkSize;
        }
        cursor = chunkData + chunkSize + (chunkSize & 1U);
    }
    if (format != 1 || channels == 0 || sampleRate == 0 || dataSize == 0 || dataOffset == 0) {
        error = "WAV must contain PCM fmt and data chunks"; return {};
    }
    if (bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        error = "Only PCM 16/24/32-bit WAV is supported"; return {};
    }
    const auto bytesPerSample = static_cast<std::size_t>(bitsPerSample / 8);
    const auto frameBytes = bytesPerSample * channels;
    if (frameBytes == 0 || dataSize % frameBytes != 0) { error = "Invalid WAV sample alignment"; return {}; }

    auto decoded = std::make_shared<DecodedAudio>();
    decoded->sampleRate = static_cast<int>(sampleRate);
    decoded->channels = channels;
    decoded->samples.resize(dataSize / bytesPerSample);
    for (std::size_t index = 0; index < decoded->samples.size(); ++index) {
        const auto offset = dataOffset + index * bytesPerSample;
        std::int32_t value = 0;
        if (bitsPerSample == 16) value = static_cast<std::int16_t>(readU16(bytes, offset));
        else if (bitsPerSample == 24) {
            value = std::to_integer<unsigned char>(bytes[offset]) |
                    (std::to_integer<unsigned char>(bytes[offset + 1]) << 8U) |
                    (std::to_integer<unsigned char>(bytes[offset + 2]) << 16U);
            if ((value & 0x00800000) != 0) value |= ~0x00FFFFFF;
        } else value = static_cast<std::int32_t>(readU32(bytes, offset));
        decoded->samples[index] = static_cast<float>(value) /
                                  static_cast<float>(std::int64_t{1} << (bitsPerSample - 1));
    }
    return decoded;
}

} // namespace puffy::audio
