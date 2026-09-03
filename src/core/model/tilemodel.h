#pragma once

#include <QAbstractListModel>
#include <QStringList>

#include <vector>

#include "core/interfaces/itaskbackend.h"
#include "core/model/tile.h"

namespace frappe
{

class ConfigFacade;
class ILauncherBackend;

/// The single ordered list everything else renders from.
///
/// Content is derived, never mutated in place: rebuild() re-queries the pinned
/// entries, the launcher backend and the task backend, diffs against the current
/// rows, and emits the model signals the difference implies. A dock has tens of
/// items, so a full re-query costs microseconds and removes an entire class of
/// state-desync bugs.
///
/// Pinned entries and running windows are two sources merged into one list:
/// pinned first, in configured order, then running applications that are not
/// already pinned. Windows are matched to tiles by app id. Minimized windows
/// follow in a region of their own, behind a separator, unless configuration
/// says they belong on their application's tile instead.
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

    /// \a tasks may be null, in which case the model shows pinned entries only.
    TileModel(ConfigFacade *config,
              const ILauncherBackend *launcher,
              const ITaskBackend *tasks = nullptr,
              QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Re-queries and diffs. Idempotent: calling it twice in a row emits nothing
    /// the second time.
    Q_INVOKABLE void rebuild();

    /// The rows holding a separator. The geometry engine needs to know which
    /// rows are rules rather than cells, and kind is the model's to report —
    /// the view would otherwise have to reach into delegates to find out.
    Q_INVOKABLE QList<int> separatorRows() const;

    /// Moves a pinned tile, persisting the new order to config. Emits rowsMoved.
    Q_INVOKABLE bool moveTile(int from, int to);

    /// Adds or removes \a tileId from the pinned entries and rebuilds. Returns
    /// false when nothing changed.
    ///
    /// The single place pinning is written. It touches configuration and
    /// nothing else: an application that is unpinned is still installed, and
    /// removing a tile must never be able to grow into removing a program.
    Q_INVOKABLE bool setPinned(const QString &tileId, bool pinned);

    /// Adds \a path to the file region, at the end, and rebuilds. Returns false
    /// when the path is already there or does not exist.
    ///
    /// The file-region counterpart to setPinned(): it touches configuration and
    /// nothing else. Removing a folder tile must never be able to grow into
    /// removing a folder.
    Q_INVOKABLE bool addFileEntry(const QString &path);

    /// Removes \a path from the file region and rebuilds. Returns false when it
    /// was not there.
    Q_INVOKABLE bool removeFileEntry(const QString &path);

    /// Removes the tile at \a row from whichever list persists it: a pinned
    /// application from the pinned entries, a file or folder from the file
    /// region. The drag-out gesture knows a row rather than an id, and asking it
    /// to translate one to the other means reaching into the model from the view.
    Q_INVOKABLE bool unpinTile(int row);

    /// Writes the current order of both orderable regions back to config.
    ///
    /// Both, not just the one that moved: rebuild() re-derives the order from
    /// config, so a region left unwritten is a region whose move is undone.
    void persistOrder();

    /// The Region \a row belongs to, as an int, or Region::Pinned for a row
    /// that does not exist. QML needs it to decide what a drop aimed there
    /// means, and a model that answers is better than a view that guesses.
    Q_INVOKABLE int regionOfRow(int row) const;

    /// The tile at \a row, for tests and for callers that want the whole struct.
    const Tile &tileAt(int row) const;

    /// Running windows that could not be matched to a desktop entry as of the
    /// last rebuild — by app id, or by window id where the compositor reported no
    /// app id at all. Collected rather than special-cased: matching fails for
    /// interpreters, wrappers and sandboxed applications, and the remedy is a
    /// general one informed by what actually turns up here.
    QStringList unmatchedIds() const;

private:
    std::vector<Tile> buildTiles(QStringList *unmatched) const;
    /// Appends one tile per configured file or folder, behind a separator.
    void appendFileRegion(std::vector<Tile> &tiles) const;
    /// Appends one tile per minimized window, behind a separator — unless the
    /// configuration says minimized windows belong on their application's tile.
    void appendMinimizedRegion(std::vector<Tile> &tiles, const std::vector<WindowInfo> &windows) const;
    void noteUnmatched(const QStringList &unmatched);

    ConfigFacade *m_config;
    const ILauncherBackend *m_launcher;
    const ITaskBackend *m_tasks;
    std::vector<Tile> m_tiles;
    QStringList m_unmatchedIds;
};

}
