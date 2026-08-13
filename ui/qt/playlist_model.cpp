#include "ui/qt/playlist_model.hpp"

#include <QVariantList>

#include <algorithm>

namespace puffy::ui {

PlaylistModel::PlaylistModel(profiles::ProfileStore& store, QObject* parent) : QAbstractListModel(parent), store_(store) { refresh(); }

int PlaylistModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : static_cast<int>(playlists_.size()); }

QVariant PlaylistModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) return {};
    const auto& playlist = playlists_[static_cast<std::size_t>(index.row())];
    if (role == PlaylistIdRole) return QVariant::fromValue<qlonglong>(playlist.id);
    if (role == NameRole) return QString::fromStdString(playlist.name);
    return {};
}

QHash<int, QByteArray> PlaylistModel::roleNames() const { return {{PlaylistIdRole, "playlistId"}, {NameRole, "name"}}; }

void PlaylistModel::refresh() {
    beginResetModel(); playlists_ = store_.playlists(); endResetModel();
    if (playlists_.empty()) currentPlaylist_ = -1;
    else if (currentPlaylist_ < 0 || currentPlaylist_ >= static_cast<int>(playlists_.size())) currentPlaylist_ = 0;
    emit playlistsChanged(); emit currentPlaylistChanged(); applySelected();
}

bool PlaylistModel::createPlaylist(const QString& name) {
    profiles::Playlist playlist; playlist.name = name.trimmed().toStdString(); if (playlist.name.empty() || !store_.savePlaylist(playlist)) return false; refresh(); return true;
}

bool PlaylistModel::renamePlaylist(int row, const QString& name) {
    if (row < 0 || row >= rowCount() || name.trimmed().isEmpty()) return false;
    if (!store_.renamePlaylist(playlists_[static_cast<std::size_t>(row)].id, name.trimmed().toStdString())) return false;
    refresh(); return true;
}

bool PlaylistModel::removePlaylist(int row) {
    if (row < 0 || row >= rowCount()) return false;
    if (!store_.removePlaylist(playlists_[static_cast<std::size_t>(row)].id)) return false;
    refresh(); return true;
}

bool PlaylistModel::addSoundToCurrent(qint64 soundId) {
    if (currentPlaylist_ < 0 || currentPlaylist_ >= rowCount() || containsSound(soundId)) return false;
    auto& playlist = playlists_[static_cast<std::size_t>(currentPlaylist_)];
    playlist.soundIds.push_back(soundId);
    const bool ok = store_.replacePlaylistItems(playlist);
    if (ok) { emit playlistsChanged(); applySelected(); }
    return ok;
}

bool PlaylistModel::removeSoundFromCurrent(qint64 soundId) {
    if (currentPlaylist_ < 0 || currentPlaylist_ >= rowCount()) return false;
    auto& playlist = playlists_[static_cast<std::size_t>(currentPlaylist_)];
    const auto oldSize = playlist.soundIds.size();
    playlist.soundIds.erase(std::remove(playlist.soundIds.begin(), playlist.soundIds.end(), soundId), playlist.soundIds.end());
    if (playlist.soundIds.size() == oldSize) return false;
    const bool ok = store_.replacePlaylistItems(playlist);
    if (ok) { emit playlistsChanged(); applySelected(); }
    return ok;
}

bool PlaylistModel::containsSound(qint64 soundId) const {
    const auto* playlist = selected();
    return playlist != nullptr && std::find(playlist->soundIds.begin(), playlist->soundIds.end(), soundId) != playlist->soundIds.end();
}

QVariantList PlaylistModel::currentSoundIds() const {
    QVariantList ids;
    if (const auto* playlist = selected(); playlist != nullptr) for (const auto id : playlist->soundIds) ids.append(QVariant::fromValue<qlonglong>(id));
    return ids;
}

bool PlaylistModel::setPlaylistSounds(int row, const QVariantList& ids) {
    if (row < 0 || row >= rowCount()) return false;
    auto& playlist = playlists_[static_cast<std::size_t>(row)]; playlist.soundIds.clear();
    for (const auto& id : ids) playlist.soundIds.push_back(id.toLongLong());
    return store_.replacePlaylistItems(playlist);
}

void PlaylistModel::setCurrentPlaylist(int row) { if (row < 0 || row >= rowCount() || row == currentPlaylist_) return; currentPlaylist_ = row; emit currentPlaylistChanged(); applySelected(); }
const profiles::Playlist* PlaylistModel::selected() const noexcept { return currentPlaylist_ >= 0 && currentPlaylist_ < static_cast<int>(playlists_.size()) ? &playlists_[static_cast<std::size_t>(currentPlaylist_)] : nullptr; }
void PlaylistModel::applySelected() { if (router_ != nullptr && selected() != nullptr) { std::vector<int> ids; for (const auto id : selected()->soundIds) ids.push_back(static_cast<int>(id)); router_->setFullKeyboardPlaylist(std::move(ids)); } }

} // namespace puffy::ui
