#include "core/profiles/profile_store.hpp"

#include <sqlite3.h>

namespace puffy::profiles {
namespace {
constexpr const char* schemaSql = R"sql(
CREATE TABLE IF NOT EXISTS profiles (
 id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE,
 full_keyboard_enabled INTEGER NOT NULL DEFAULT 0, full_keyboard_mode INTEGER NOT NULL DEFAULT 0,
 single_sound_id INTEGER NOT NULL DEFAULT -1, avoid_repeats INTEGER NOT NULL DEFAULT 1,
 trigger_on_repeat INTEGER NOT NULL DEFAULT 0, ignore_ctrl INTEGER NOT NULL DEFAULT 0,
 ignore_shift INTEGER NOT NULL DEFAULT 0, ignore_alt INTEGER NOT NULL DEFAULT 0,
 ignore_super INTEGER NOT NULL DEFAULT 0, ignored_key_codes TEXT NOT NULL DEFAULT ''
);
CREATE TABLE IF NOT EXISTS playlists (
 id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS playlist_items (
 playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,
 position INTEGER NOT NULL, sound_id INTEGER NOT NULL, PRIMARY KEY (playlist_id, position)
);
)sql";

void text(sqlite3_stmt* statement, int index, const std::string& value) { sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT); }
}

ProfileStore::ProfileStore(std::string databasePath) : path_(std::move(databasePath)) {}
ProfileStore::~ProfileStore() { if (database_) sqlite3_close(database_); }

bool ProfileStore::fail(std::string message) const { error_ = std::move(message); return false; }

bool ProfileStore::open() {
    if (database_) return true;
    if (sqlite3_open(path_.c_str(), &database_) != SQLITE_OK) { const auto message = database_ ? sqlite3_errmsg(database_) : "sqlite open failed"; if (database_) sqlite3_close(database_); database_ = nullptr; return fail(message); }
    sqlite3_busy_timeout(database_, 2000);
    return schema();
}

bool ProfileStore::schema() {
    char* error = nullptr;
    if (sqlite3_exec(database_, schemaSql, nullptr, nullptr, &error) != SQLITE_OK) { const std::string message = error ? error : "profile schema failed"; sqlite3_free(error); return fail(message); }
    return true;
}

bool ProfileStore::saveProfile(Profile& profile) {
    if (!database_) return fail("ProfileStore is not open");
    sqlite3_stmt* statement = nullptr;
    constexpr const char* sql = "INSERT INTO profiles (name,full_keyboard_enabled,full_keyboard_mode,single_sound_id,avoid_repeats,trigger_on_repeat,ignore_ctrl,ignore_shift,ignore_alt,ignore_super,ignored_key_codes) VALUES (?,?,?,?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) return fail(sqlite3_errmsg(database_));
    text(statement, 1, profile.name); sqlite3_bind_int(statement, 2, profile.fullKeyboardEnabled); sqlite3_bind_int(statement, 3, static_cast<int>(profile.fullKeyboardMode)); sqlite3_bind_int64(statement, 4, profile.singleSoundId); sqlite3_bind_int(statement, 5, profile.avoidImmediateRepeats); sqlite3_bind_int(statement, 6, profile.triggerOnRepeat); sqlite3_bind_int(statement, 7, profile.ignoreCtrl); sqlite3_bind_int(statement, 8, profile.ignoreShift); sqlite3_bind_int(statement, 9, profile.ignoreAlt); sqlite3_bind_int(statement, 10, profile.ignoreSuper); text(statement, 11, profile.ignoredKeyCodes);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (ok) profile.id = sqlite3_last_insert_rowid(database_); else fail(sqlite3_errmsg(database_));
    sqlite3_finalize(statement); return ok;
}

bool ProfileStore::updateProfile(const Profile& profile) {
    if (!database_) return fail("ProfileStore is not open");
    sqlite3_stmt* statement = nullptr;
    constexpr const char* sql = "UPDATE profiles SET name=?,full_keyboard_enabled=?,full_keyboard_mode=?,single_sound_id=?,avoid_repeats=?,trigger_on_repeat=?,ignore_ctrl=?,ignore_shift=?,ignore_alt=?,ignore_super=?,ignored_key_codes=? WHERE id=?";
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) return fail(sqlite3_errmsg(database_));
    text(statement, 1, profile.name); sqlite3_bind_int(statement, 2, profile.fullKeyboardEnabled); sqlite3_bind_int(statement, 3, static_cast<int>(profile.fullKeyboardMode)); sqlite3_bind_int64(statement, 4, profile.singleSoundId); sqlite3_bind_int(statement, 5, profile.avoidImmediateRepeats); sqlite3_bind_int(statement, 6, profile.triggerOnRepeat); sqlite3_bind_int(statement, 7, profile.ignoreCtrl); sqlite3_bind_int(statement, 8, profile.ignoreShift); sqlite3_bind_int(statement, 9, profile.ignoreAlt); sqlite3_bind_int(statement, 10, profile.ignoreSuper); text(statement, 11, profile.ignoredKeyCodes); sqlite3_bind_int64(statement, 12, profile.id);
    const bool ok = sqlite3_step(statement) == SQLITE_DONE; if (!ok) fail(sqlite3_errmsg(database_)); sqlite3_finalize(statement); return ok;
}

std::vector<Profile> ProfileStore::profiles() const {
    std::vector<Profile> result; if (!database_) return result; sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT id,name,full_keyboard_enabled,full_keyboard_mode,single_sound_id,avoid_repeats,trigger_on_repeat,ignore_ctrl,ignore_shift,ignore_alt,ignore_super,ignored_key_codes FROM profiles ORDER BY name", -1, &statement, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(statement) == SQLITE_ROW) { Profile p; p.id = sqlite3_column_int64(statement, 0); p.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)); p.fullKeyboardEnabled = sqlite3_column_int(statement, 2); p.fullKeyboardMode = static_cast<soundboard::FullKeyboardMode>(sqlite3_column_int(statement, 3)); p.singleSoundId = sqlite3_column_int64(statement, 4); p.avoidImmediateRepeats = sqlite3_column_int(statement, 5); p.triggerOnRepeat = sqlite3_column_int(statement, 6); p.ignoreCtrl = sqlite3_column_int(statement, 7); p.ignoreShift = sqlite3_column_int(statement, 8); p.ignoreAlt = sqlite3_column_int(statement, 9); p.ignoreSuper = sqlite3_column_int(statement, 10); p.ignoredKeyCodes = reinterpret_cast<const char*>(sqlite3_column_text(statement, 11)); result.push_back(std::move(p)); }
    sqlite3_finalize(statement); return result;
}

bool ProfileStore::savePlaylist(Playlist& playlist) {
    if (!database_) return fail("ProfileStore is not open");
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "INSERT INTO playlists (name) VALUES (?)", -1, &statement, nullptr) != SQLITE_OK) return fail(sqlite3_errmsg(database_));
    text(statement, 1, playlist.name); const bool ok = sqlite3_step(statement) == SQLITE_DONE; if (ok) playlist.id = sqlite3_last_insert_rowid(database_); else fail(sqlite3_errmsg(database_)); sqlite3_finalize(statement); if (!ok) return false; return replacePlaylistItems(playlist);
}

bool ProfileStore::replacePlaylistItems(const Playlist& playlist) {
    if (!database_) return fail("ProfileStore is not open");
    sqlite3_exec(database_, "BEGIN", nullptr, nullptr, nullptr);
    sqlite3_stmt* deleteStatement = nullptr; sqlite3_prepare_v2(database_, "DELETE FROM playlist_items WHERE playlist_id=?", -1, &deleteStatement, nullptr); sqlite3_bind_int64(deleteStatement, 1, playlist.id); sqlite3_step(deleteStatement); sqlite3_finalize(deleteStatement);
    sqlite3_stmt* insertStatement = nullptr; if (sqlite3_prepare_v2(database_, "INSERT INTO playlist_items (playlist_id,position,sound_id) VALUES (?,?,?)", -1, &insertStatement, nullptr) != SQLITE_OK) { sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr); return fail(sqlite3_errmsg(database_)); }
    for (std::size_t position = 0; position < playlist.soundIds.size(); ++position) { sqlite3_bind_int64(insertStatement, 1, playlist.id); sqlite3_bind_int(insertStatement, 2, static_cast<int>(position)); sqlite3_bind_int64(insertStatement, 3, playlist.soundIds[position]); if (sqlite3_step(insertStatement) != SQLITE_DONE) { sqlite3_finalize(insertStatement); sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr); return fail(sqlite3_errmsg(database_)); } sqlite3_reset(insertStatement); }
    sqlite3_finalize(insertStatement); sqlite3_exec(database_, "COMMIT", nullptr, nullptr, nullptr); return true;
}

std::vector<Playlist> ProfileStore::playlists() const {
    std::vector<Playlist> result; if (!database_) return result; sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT id,name FROM playlists ORDER BY name", -1, &statement, nullptr) != SQLITE_OK) return result;
    while (sqlite3_step(statement) == SQLITE_ROW) { Playlist p; p.id = sqlite3_column_int64(statement, 0); p.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1)); sqlite3_stmt* items = nullptr; sqlite3_prepare_v2(database_, "SELECT sound_id FROM playlist_items WHERE playlist_id=? ORDER BY position", -1, &items, nullptr); sqlite3_bind_int64(items, 1, p.id); while (sqlite3_step(items) == SQLITE_ROW) p.soundIds.push_back(sqlite3_column_int64(items, 0)); sqlite3_finalize(items); result.push_back(std::move(p)); }
    sqlite3_finalize(statement); return result;
}

} // namespace puffy::profiles
