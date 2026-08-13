#pragma once

#include "core/profiles/profile_store.hpp"
#include "core/hotkeys/hotkey_router.hpp"

#include <QAbstractListModel>

namespace puffy::ui {

class PlaylistModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int currentPlaylist READ currentPlaylist WRITE setCurrentPlaylist NOTIFY currentPlaylistChanged)

public:
    enum Roles { PlaylistIdRole = Qt::UserRole + 1, NameRole };
    explicit PlaylistModel(profiles::ProfileStore& store, QObject* parent = nullptr);
    void setHotkeyRouter(hotkeys::HotkeyRouter* router) noexcept { router_ = router; applySelected(); }
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool createPlaylist(const QString& name);
    Q_INVOKABLE bool setPlaylistSounds(int row, const QVariantList& ids);
    int currentPlaylist() const noexcept { return currentPlaylist_; }
    void setCurrentPlaylist(int row);
    [[nodiscard]] const profiles::Playlist* selected() const noexcept;

signals:
    void currentPlaylistChanged();
    void playlistsChanged();

private:
    profiles::ProfileStore& store_;
    std::vector<profiles::Playlist> playlists_;
    int currentPlaylist_{-1};
    hotkeys::HotkeyRouter* router_{nullptr};
    void applySelected();
};

} // namespace puffy::ui
