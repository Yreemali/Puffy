#pragma once

#include "core/audio/audio_types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace puffy::library {

enum class PlaybackMode : std::uint8_t {
    Restart,
    Overlap,
    IgnoreIfPlaying,
    Toggle,
    Hold,
};

struct Sound {
    std::int64_t id{0};
    std::string name;
    std::filesystem::path filePath;
    double durationSeconds{0.0};
    float volume{1.0F};
    std::string category;
    std::string tags;
    bool favorite{false};
    std::string hotkey;
    PlaybackMode playbackMode{PlaybackMode::Overlap};
    audio::OutputRoute route{audio::OutputRoute::Both};
    float pitch{1.0F};
    float speed{1.0F};
    float fadeInSeconds{0.0F};
    float fadeOutSeconds{0.0F};
    bool loop{false};
};

class SoundLibrary final {
public:
    explicit SoundLibrary(std::filesystem::path databasePath);
    ~SoundLibrary();

    SoundLibrary(const SoundLibrary&) = delete;
    SoundLibrary& operator=(const SoundLibrary&) = delete;

    [[nodiscard]] bool open();
    [[nodiscard]] bool add(Sound& sound);
    [[nodiscard]] bool update(const Sound& sound);
    [[nodiscard]] bool remove(std::int64_t id);
    [[nodiscard]] std::optional<Sound> find(std::int64_t id) const;
    [[nodiscard]] std::vector<Sound> search(std::string_view query) const;
    [[nodiscard]] std::vector<Sound> all() const;
    [[nodiscard]] const std::string& lastError() const noexcept { return lastError_; }

private:
    bool executeSchema();
    bool setError(std::string message) const;
    [[nodiscard]] std::optional<Sound> readSound(void* statement) const;

    std::filesystem::path databasePath_;
    sqlite3* database_{nullptr};
    mutable std::string lastError_;
};

} // namespace puffy::library
