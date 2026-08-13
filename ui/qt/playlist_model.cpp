#include "ui/qt/playlist_model.hpp"

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
