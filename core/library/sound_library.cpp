#include "core/library/sound_library.hpp"

#include <sqlite3.h>

#include <string_view>

namespace puffy::library {
namespace {

constexpr const char* schema = R"sql(
CREATE TABLE IF NOT EXISTS sounds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    file_path TEXT NOT NULL UNIQUE,
    duration_seconds REAL NOT NULL DEFAULT 0,
    volume REAL NOT NULL DEFAULT 1,
    category TEXT NOT NULL DEFAULT '',
    tags TEXT NOT NULL DEFAULT '',
    favorite INTEGER NOT NULL DEFAULT 0,
    hotkey TEXT NOT NULL DEFAULT '',
    playback_mode INTEGER NOT NULL DEFAULT 1,
    route INTEGER NOT NULL DEFAULT 3,
    pitch REAL NOT NULL DEFAULT 1,
    speed REAL NOT NULL DEFAULT 1,
    fade_in_seconds REAL NOT NULL DEFAULT 0,
    fade_out_seconds REAL NOT NULL DEFAULT 0,
    loop INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_sounds_name ON sounds(name);
CREATE INDEX IF NOT EXISTS idx_sounds_category ON sounds(category);
)sql";

void bindText(sqlite3_stmt* statement, int index, const std::string& value) {
    sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

} // namespace

SoundLibrary::SoundLibrary(std::filesystem::path databasePath) : databasePath_(std::move(databasePath)) {}

SoundLibrary::~SoundLibrary() {
    if (database_ != nullptr) sqlite3_close(database_);
}

bool SoundLibrary::setError(std::string message) const {
    lastError_ = std::move(message);
    return false;
}

bool SoundLibrary::open() {
    if (database_ != nullptr) return true;
    if (databasePath_.has_parent_path()) {
        std::error_code error;
        std::filesystem::create_directories(databasePath_.parent_path(), error);
        if (error) return setError("Cannot create database directory: " + error.message());
    }
    if (sqlite3_open(databasePath_.string().c_str(), &database_) != SQLITE_OK) {
        const std::string message = database_ ? sqlite3_errmsg(database_) : "sqlite3_open failed";
        if (database_) sqlite3_close(database_);
        database_ = nullptr;
        return setError(message);
    }
    sqlite3_busy_timeout(database_, 2000);
    return executeSchema();
}

bool SoundLibrary::executeSchema() {
    char* errorMessage = nullptr;
    if (sqlite3_exec(database_, schema, nullptr, nullptr, &errorMessage) != SQLITE_OK) {
        const std::string message = errorMessage ? errorMessage : "schema creation failed";
        sqlite3_free(errorMessage);
        return setError(message);
    }
    return true;
}

bool SoundLibrary::add(Sound& sound) {
    if (database_ == nullptr) return setError("SoundLibrary is not open");
    constexpr const char* sql = "INSERT INTO sounds (name,file_path,duration_seconds,volume,category,tags,favorite,hotkey,playback_mode,route,pitch,speed,fade_in_seconds,fade_out_seconds,loop) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) return setError(sqlite3_errmsg(database_));
    bindText(statement, 1, sound.name);
    bindText(statement, 2, sound.filePath.string());
    sqlite3_bind_double(statement, 3, sound.durationSeconds);
    sqlite3_bind_double(statement, 4, sound.volume);
    bindText(statement, 5, sound.category);
    bindText(statement, 6, sound.tags);
    sqlite3_bind_int(statement, 7, sound.favorite);
    bindText(statement, 8, sound.hotkey);
    sqlite3_bind_int(statement, 9, static_cast<int>(sound.playbackMode));
    sqlite3_bind_int(statement, 10, static_cast<int>(sound.route));
    sqlite3_bind_double(statement, 11, sound.pitch);
    sqlite3_bind_double(statement, 12, sound.speed);
    sqlite3_bind_double(statement, 13, sound.fadeInSeconds);
    sqlite3_bind_double(statement, 14, sound.fadeOutSeconds);
    sqlite3_bind_int(statement, 15, sound.loop);
    const bool success = sqlite3_step(statement) == SQLITE_DONE;
    if (!success) setError(sqlite3_errmsg(database_));
    else sound.id = sqlite3_last_insert_rowid(database_);
    sqlite3_finalize(statement);
    return success;
}

bool SoundLibrary::update(const Sound& sound) {
    if (database_ == nullptr) return setError("SoundLibrary is not open");
    constexpr const char* sql = "UPDATE sounds SET name=?,file_path=?,duration_seconds=?,volume=?,category=?,tags=?,favorite=?,hotkey=?,playback_mode=?,route=?,pitch=?,speed=?,fade_in_seconds=?,fade_out_seconds=?,loop=? WHERE id=?";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) return setError(sqlite3_errmsg(database_));
    bindText(statement, 1, sound.name); bindText(statement, 2, sound.filePath.string());
    sqlite3_bind_double(statement, 3, sound.durationSeconds); sqlite3_bind_double(statement, 4, sound.volume);
    bindText(statement, 5, sound.category); bindText(statement, 6, sound.tags); sqlite3_bind_int(statement, 7, sound.favorite);
    bindText(statement, 8, sound.hotkey); sqlite3_bind_int(statement, 9, static_cast<int>(sound.playbackMode));
    sqlite3_bind_int(statement, 10, static_cast<int>(sound.route)); sqlite3_bind_double(statement, 11, sound.pitch);
    sqlite3_bind_double(statement, 12, sound.speed); sqlite3_bind_double(statement, 13, sound.fadeInSeconds);
    sqlite3_bind_double(statement, 14, sound.fadeOutSeconds); sqlite3_bind_int(statement, 15, sound.loop);
    sqlite3_bind_int64(statement, 16, sound.id);
    const bool success = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(database_) == 1;
    if (!success) setError(success ? "Sound not found" : sqlite3_errmsg(database_));
    sqlite3_finalize(statement);
    return success;
}

bool SoundLibrary::remove(std::int64_t id) {
    if (database_ == nullptr) return setError("SoundLibrary is not open");
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "DELETE FROM sounds WHERE id=?", -1, &statement, nullptr) != SQLITE_OK) return setError(sqlite3_errmsg(database_));
    sqlite3_bind_int64(statement, 1, id);
    const bool success = sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(database_) == 1;
    if (!success) setError(success ? "Sound not found" : sqlite3_errmsg(database_));
    sqlite3_finalize(statement);
    return success;
}

std::optional<Sound> SoundLibrary::readSound(void* rawStatement) const {
    auto* statement = static_cast<sqlite3_stmt*>(rawStatement);
    Sound sound;
    sound.id = sqlite3_column_int64(statement, 0);
    sound.name = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    sound.filePath = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    sound.durationSeconds = sqlite3_column_double(statement, 3); sound.volume = static_cast<float>(sqlite3_column_double(statement, 4));
    sound.category = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5)); sound.tags = reinterpret_cast<const char*>(sqlite3_column_text(statement, 6));
    sound.favorite = sqlite3_column_int(statement, 7) != 0; sound.hotkey = reinterpret_cast<const char*>(sqlite3_column_text(statement, 8));
    sound.playbackMode = static_cast<PlaybackMode>(sqlite3_column_int(statement, 9)); sound.route = static_cast<audio::OutputRoute>(sqlite3_column_int(statement, 10));
    sound.pitch = static_cast<float>(sqlite3_column_double(statement, 11)); sound.speed = static_cast<float>(sqlite3_column_double(statement, 12));
    sound.fadeInSeconds = static_cast<float>(sqlite3_column_double(statement, 13)); sound.fadeOutSeconds = static_cast<float>(sqlite3_column_double(statement, 14));
    sound.loop = sqlite3_column_int(statement, 15) != 0;
    return sound;
}

std::optional<Sound> SoundLibrary::find(std::int64_t id) const {
    if (database_ == nullptr) return std::nullopt;
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, "SELECT id,name,file_path,duration_seconds,volume,category,tags,favorite,hotkey,playback_mode,route,pitch,speed,fade_in_seconds,fade_out_seconds,loop FROM sounds WHERE id=?", -1, &statement, nullptr) != SQLITE_OK) return std::nullopt;
    sqlite3_bind_int64(statement, 1, id);
    std::optional<Sound> result;
    if (sqlite3_step(statement) == SQLITE_ROW) result = readSound(statement);
    sqlite3_finalize(statement);
    return result;
}

std::vector<Sound> SoundLibrary::all() const { return search(""); }

std::vector<Sound> SoundLibrary::search(std::string_view query) const {
    std::vector<Sound> result;
    if (database_ == nullptr) return result;
    constexpr const char* sql = "SELECT id,name,file_path,duration_seconds,volume,category,tags,favorite,hotkey,playback_mode,route,pitch,speed,fade_in_seconds,fade_out_seconds,loop FROM sounds WHERE ?='' OR name LIKE '%' || ? || '%' OR category LIKE '%' || ? || '%' OR tags LIKE '%' || ? || '%' ORDER BY favorite DESC, name COLLATE NOCASE";
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) return result;
    const std::string text(query);
    for (int index = 1; index <= 4; ++index) bindText(statement, index, text);
    while (sqlite3_step(statement) == SQLITE_ROW) result.push_back(*readSound(statement));
    sqlite3_finalize(statement);
    return result;
}

} // namespace puffy::library
