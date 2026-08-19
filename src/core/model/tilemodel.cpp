#include "core/model/tilemodel.h"

#include "core/config/configfacade.h"
#include "core/interfaces/ilauncherbackend.h"
#include "core/interfaces/itaskbackend.h"

#include <QLoggingCategory>

#include <algorithm>

using namespace frappe;

namespace
{
Q_LOGGING_CATEGORY(FRAPPE_TASKS, "frappe.tasks")

/// Shown when a pinned entry no longer resolves to an installed application. A
/// placeholder tile is deliberately preferred to a gap: the user pinned it, and
/// silently dropping it hides the fact that the application went away.
constexpr auto placeholderIcon = "application-x-executable";

/// Stable id for the rule between the application region and the minimized one.
/// The diff keys on ids, so it needs one even though nothing can be done to it.
constexpr auto minimizedSeparatorId = "separator:minimized";

int indexOfId(const std::vector<Tile> &tiles, const QString &id, int from)
{
    for (int i = from; i < static_cast<int>(tiles.size()); ++i) {
        if (tiles[i].id == id) {
            return i;
        }
    }
    return -1;
}

/// Running window counts per app id, in the order the apps were first seen.
///
/// A vector rather than a hash: there are tens of windows at most, and the order
/// is load-bearing — it decides where unpinned running apps land in the dock, so
/// a hash's arbitrary iteration order would make the layout jump about.
std::vector<std::pair<QString, int>> countByApp(const std::vector<WindowInfo> &windows, QStringList *unnamed)
{
    std::vector<std::pair<QString, int>> counts;
    for (const WindowInfo &window : windows) {
        if (window.appId.isEmpty()) {
            // Nothing to aggregate on and nothing to draw: a window the
            // compositor gave no app id for cannot be merged with anything.
            if (unnamed && !unnamed->contains(window.windowId)) {
                unnamed->append(window.windowId);
            }
            continue;
        }

        const auto it = std::find_if(counts.begin(), counts.end(), [&window](const auto &entry) {
            return entry.first == window.appId;
        });
        if (it == counts.end()) {
            counts.emplace_back(window.appId, 1);
        } else {
            ++it->second;
        }
    }
    return counts;
}

int countFor(const std::vector<std::pair<QString, int>> &counts, const QString &appId)
{
    const auto it = std::find_if(counts.begin(), counts.end(), [&appId](const auto &entry) {
        return entry.first == appId;
    });
    return it == counts.end() ? 0 : it->second;
}
}

TileModel::TileModel(ConfigFacade *config, const ILauncherBackend *launcher, const ITaskBackend *tasks, QObject *parent)
    : QAbstractListModel(parent)
    , m_config(config)
    , m_launcher(launcher)
    , m_tasks(tasks)
{
    QStringList unmatched;
    m_tiles = buildTiles(&unmatched);
    noteUnmatched(unmatched);
}

QList<int> TileModel::separatorRows() const
{
    QList<int> rows;
    for (std::size_t i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles[i].kind == TileKind::Separator) {
            rows.append(int(i));
        }
    }
    return rows;
}

int TileModel::rowCount(const QModelIndex &parent) const
{
    // A list model has no children, so anything under a valid parent is empty.
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_tiles.size());
}

QVariant TileModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_tiles.size())) {
        return {};
    }

    const Tile &tile = m_tiles[index.row()];
    switch (role) {
    case KindRole:
        return static_cast<int>(tile.kind);
    case RegionRole:
        return static_cast<int>(tile.region);
    case IdRole:
        return tile.id;
    case NameRole:
        return tile.name;
    case IconNameRole:
        return tile.iconName;
    case IsPinnedRole:
        return tile.isPinned;
    case IsRunningRole:
        return tile.isRunning;
    case WindowCountRole:
        return tile.windowCount;
    case BadgeRole:
        return tile.badgeCount ? QVariant(*tile.badgeCount) : QVariant();
    case ProgressRole:
        return tile.progress ? QVariant(*tile.progress) : QVariant();
    default:
        return {};
    }
}

QHash<int, QByteArray> TileModel::roleNames() const
{
    return {
        {KindRole, QByteArrayLiteral("kind")},
        {RegionRole, QByteArrayLiteral("region")},
        {IdRole, QByteArrayLiteral("tileId")},
        {NameRole, QByteArrayLiteral("name")},
        {IconNameRole, QByteArrayLiteral("iconName")},
        {IsPinnedRole, QByteArrayLiteral("isPinned")},
        {IsRunningRole, QByteArrayLiteral("isRunning")},
        {WindowCountRole, QByteArrayLiteral("windowCount")},
        {BadgeRole, QByteArrayLiteral("badgeCount")},
        {ProgressRole, QByteArrayLiteral("progress")},
    };
}

const Tile &TileModel::tileAt(int row) const
{
    return m_tiles.at(row);
}

QStringList TileModel::unmatchedIds() const
{
    return m_unmatchedIds;
}

std::vector<Tile> TileModel::buildTiles(QStringList *unmatched) const
{
    const std::vector<WindowInfo> windows = m_tasks ? m_tasks->windows() : std::vector<WindowInfo>();
    const std::vector<std::pair<QString, int>> counts = countByApp(windows, unmatched);

    std::vector<Tile> tiles;
    const QStringList pinned = m_config ? m_config->pinnedEntries() : QStringList();
    tiles.reserve(pinned.size() + static_cast<qsizetype>(counts.size()));

    for (const QString &id : pinned) {
        Tile tile;
        tile.kind = TileKind::Application;
        tile.region = Region::Pinned;
        tile.id = id;
        tile.isPinned = true;
        tile.windowCount = countFor(counts, id);
        tile.isRunning = tile.windowCount > 0;

        const auto entry = m_launcher ? m_launcher->lookup(id) : std::unexpected(Error::NotFound);
        if (entry) {
            tile.name = entry->name;
            tile.iconName = entry->iconName;
        } else {
            tile.name = id;
            tile.iconName = QString::fromLatin1(placeholderIcon);
        }
        tiles.push_back(std::move(tile));
    }

    // Running applications that are not pinned, appended in first-seen order and
    // tagged Pinned: they share the band with the pinned tiles and differ only in
    // that they disappear when their last window closes.
    for (const auto &[appId, count] : counts) {
        if (pinned.contains(appId)) {
            continue;
        }

        Tile tile;
        tile.kind = TileKind::Application;
        tile.region = Region::Pinned;
        tile.id = appId;
        tile.isPinned = false;
        tile.isRunning = true;
        tile.windowCount = count;

        const auto entry = m_launcher ? m_launcher->lookup(appId) : std::unexpected(Error::NotFound);
        if (entry) {
            tile.name = entry->name;
            tile.iconName = entry->iconName;
        } else {
            // App-id matching failed. The window is real and running, so it still
            // gets a tile — degrading to the raw id is better than pretending the
            // application is not there.
            tile.name = appId;
            tile.iconName = QString::fromLatin1(placeholderIcon);
            if (unmatched && !unmatched->contains(appId)) {
                unmatched->append(appId);
            }
        }
        tiles.push_back(std::move(tile));
    }

    appendMinimizedRegion(tiles, windows);
    return tiles;
}

void TileModel::appendMinimizedRegion(std::vector<Tile> &tiles, const std::vector<WindowInfo> &windows) const
{
    // "Minimize into icon" means there is no region: the window is represented
    // by its application's own tile, which is already there.
    if (m_config && m_config->minimizeIntoIcon()) {
        return;
    }

    std::vector<Tile> minimized;
    for (const WindowInfo &window : windows) {
        if (!window.isMinimized || window.appId.isEmpty()) {
            continue;
        }

        Tile tile;
        tile.kind = TileKind::MinimizedWindow;
        tile.region = Region::Minimized;
        // Keyed on the window, not the application: two minimized windows of one
        // application are two tiles, and the diff has to tell them apart.
        tile.id = window.windowId;
        tile.name = window.title.isEmpty() ? window.appId : window.title;
        tile.isRunning = true;
        tile.windowCount = 1;

        const auto entry = m_launcher ? m_launcher->lookup(window.appId) : std::unexpected(Error::NotFound);
        tile.iconName = entry ? entry->iconName : QString::fromLatin1(placeholderIcon);
        minimized.push_back(std::move(tile));
    }

    if (minimized.empty() || tiles.empty()) {
        return;
    }

    // The rule that makes the dock region-structured rather than one long row:
    // a separator exists only between two non-empty regions.
    Tile separator;
    separator.kind = TileKind::Separator;
    separator.region = Region::Minimized;
    separator.id = QString::fromLatin1(minimizedSeparatorId);
    tiles.push_back(std::move(separator));

    tiles.insert(tiles.end(), std::make_move_iterator(minimized.begin()), std::make_move_iterator(minimized.end()));
}

void TileModel::noteUnmatched(const QStringList &unmatched)
{
    // rebuild() runs on every window change, so log only what is newly unmatched
    // rather than the same ids on every keystroke that opens a window.
    for (const QString &id : unmatched) {
        if (!m_unmatchedIds.contains(id)) {
            qCInfo(FRAPPE_TASKS) << "no desktop entry for running window" << id;
        }
    }
    m_unmatchedIds = unmatched;
}

void TileModel::rebuild()
{
    QStringList unmatched;
    const std::vector<Tile> next = buildTiles(&unmatched);
    noteUnmatched(unmatched);

    // Walk both lists in step. Where ids agree, the row survives and only its
    // data may have changed; where they disagree, the old id either reappears
    // later in the new list (so a row was inserted here) or it does not (so this
    // row was removed). One row at a time is fine at dock scale and is far
    // easier to get right than a batched diff — and getting it wrong corrupts
    // views silently.
    int i = 0;
    while (true) {
        const int oldCount = static_cast<int>(m_tiles.size());
        const int newCount = static_cast<int>(next.size());

        if (i >= oldCount && i >= newCount) {
            break;
        }

        if (i >= newCount) {
            beginRemoveRows(QModelIndex(), i, oldCount - 1);
            m_tiles.erase(m_tiles.begin() + i, m_tiles.end());
            endRemoveRows();
            break;
        }

        if (i >= oldCount) {
            beginInsertRows(QModelIndex(), i, newCount - 1);
            m_tiles.insert(m_tiles.end(), next.begin() + i, next.end());
            endInsertRows();
            break;
        }

        if (m_tiles[i].id == next[i].id) {
            if (m_tiles[i] != next[i]) {
                m_tiles[i] = next[i];
                const QModelIndex idx = index(i, 0);
                Q_EMIT dataChanged(idx, idx);
            }
            ++i;
            continue;
        }

        if (indexOfId(next, m_tiles[i].id, i) >= 0) {
            beginInsertRows(QModelIndex(), i, i);
            m_tiles.insert(m_tiles.begin() + i, next[i]);
            endInsertRows();
            ++i;
        } else {
            beginRemoveRows(QModelIndex(), i, i);
            m_tiles.erase(m_tiles.begin() + i);
            endRemoveRows();
        }
    }
}

bool TileModel::moveTile(int from, int to)
{
    const int count = static_cast<int>(m_tiles.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return false;
    }

    // beginMoveRows takes the destination as a position *before* the removal is
    // applied, so a forward move needs one added to it.
    const int destination = to > from ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destination)) {
        return false;
    }

    Tile moved = m_tiles[from];
    m_tiles.erase(m_tiles.begin() + from);
    m_tiles.insert(m_tiles.begin() + to, std::move(moved));
    endMoveRows();

    if (m_config) {
        QStringList pinned;
        pinned.reserve(static_cast<qsizetype>(m_tiles.size()));
        for (const Tile &tile : m_tiles) {
            if (tile.isPinned) {
                pinned.append(tile.id);
            }
        }
        m_config->setPinnedEntries(pinned);
    }

    return true;
}
