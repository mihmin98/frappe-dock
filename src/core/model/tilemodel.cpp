#include "core/model/tilemodel.h"

#include "core/config/configfacade.h"
#include "core/interfaces/ilauncherbackend.h"

#include <algorithm>

using namespace frappe;

namespace
{
/// Shown when a pinned entry no longer resolves to an installed application. A
/// placeholder tile is deliberately preferred to a gap: the user pinned it, and
/// silently dropping it hides the fact that the application went away.
constexpr auto placeholderIcon = "application-x-executable";

int indexOfId(const std::vector<Tile> &tiles, const QString &id, int from)
{
    for (int i = from; i < static_cast<int>(tiles.size()); ++i) {
        if (tiles[i].id == id) {
            return i;
        }
    }
    return -1;
}
}

TileModel::TileModel(ConfigFacade *config, const ILauncherBackend *launcher, QObject *parent)
    : QAbstractListModel(parent)
    , m_config(config)
    , m_launcher(launcher)
{
    m_tiles = buildTiles();
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

std::vector<Tile> TileModel::buildTiles() const
{
    std::vector<Tile> tiles;
    const QStringList pinned = m_config ? m_config->pinnedEntries() : QStringList();
    tiles.reserve(pinned.size());

    for (const QString &id : pinned) {
        Tile tile;
        tile.kind = TileKind::Application;
        tile.region = Region::Pinned;
        tile.id = id;
        tile.isPinned = true;

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

    return tiles;
}

void TileModel::rebuild()
{
    const std::vector<Tile> next = buildTiles();

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
