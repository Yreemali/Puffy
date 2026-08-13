#pragma once

#include "core/soundboard/full_keyboard_mode.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;

namespace puffy::profiles {

struct Profile {
    std::int64_t id{0};
    std::string name{"Default"};
    bool fullKeyboardEnabled{false};
    soundboard::FullKeyboardMode fullKeyboardMode{soundboard::FullKeyboardMode::Random};
    std::int64_t singleSoundId{-1};
    bool avoidImmediateRepeats{true};
    bool triggerOnRepeat{false};
    bool ignoreCtrl{false};
    bool ignoreShift{false};
    bool ignoreAlt{false};
    bool ignoreSuper{false};
    std::string ignoredKeyCodes;
};

struct Playlist {
    std::int64_t id{0};
    std::string name;
    std::vector<std::int64_t> soundIds;
};

class ProfileStore final {
public:
    explicit ProfileStore(std::string databasePath);
    ~ProfileStore();
    ProfileStore(const ProfileStore&) = delete;
    ProfileStore& operator=(const ProfileStore&) = delete;

    bool open();
    bool saveProfile(Profile& profile);
    bool updateProfile(const Profile& profile);
    [[nodiscard]] std::vector<Profile> profiles() const;
    bool savePlaylist(Playlist& playlist);
    bool replacePlaylistItems(const Playlist& playlist);
    [[nodiscard]] std::vector<Playlist> playlists() const;
    [[nodiscard]] const std::string& lastError() const noexcept { return error_; }

private:
    bool schema();
    bool fail(std::string message) const;
    std::string path_;
    sqlite3* database_{nullptr};
    mutable std::string error_;
};

} // namespace puffy::profiles
