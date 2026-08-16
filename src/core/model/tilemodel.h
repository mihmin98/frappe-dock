#pragma once

#include <QAbstractListModel>

#include <vector>

#include "core/model/tile.h"

namespace frappe
{

class ConfigFacade;
class ILauncherBackend;

/// The single ordered list everything else renders from.
///
/// Content is derived, never mutated in place: rebuild() re-queries the pinned
/// entries and the launcher backend, diffs against the current rows, and emits
/// the model signals the difference implies. A dock has tens of items, so a full
/// re-query costs microseconds and removes an entire class of state-desync bugs.
class TileModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        KindRole = Qt::UserRole + 1,
        RegionRole,
        IdRole,
        NameRole,
        IconNameRole,
        IsPinnedRole,
        IsRunningRole,
        WindowCountRole,
        BadgeRole,
        ProgressRole,
    };
    Q_ENUM(Roles)

    TileModel(ConfigFacade *config, const ILauncherBackend *launcher, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Re-queries and diffs. Idempotent: calling it twice in a row emits nothing
    /// the second time.
    Q_INVOKABLE void rebuild();

    /// Moves a pinned tile, persisting the new order to config. Emits rowsMoved.
    Q_INVOKABLE bool moveTile(int from, int to);

    /// The tile at \a row, for tests and for callers that want the whole struct.
    const Tile &tileAt(int row) const;

private:
    std::vector<Tile> buildTiles() const;

    ConfigFacade *m_config;
    const ILauncherBackend *m_launcher;
    std::vector<Tile> m_tiles;
};

}
