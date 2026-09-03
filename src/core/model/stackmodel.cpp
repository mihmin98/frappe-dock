#include "core/model/stackmodel.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace frappe
{

namespace
{

/// Human-order name comparison: case-insensitive, and numbers compare as
/// numbers so "img2" precedes "img10". Returns <0, 0 or >0.
int compareName(const FolderEntry &a, const FolderEntry &b)
{
    static const QCollator collator = [] {
        QCollator c;
        c.setCaseSensitivity(Qt::CaseInsensitive);
        c.setNumericMode(true);
        return c;
    }();

    const int cmp = collator.compare(a.name, b.name);
    if (cmp != 0) {
        return cmp;
    }
    // Exact name last, so the order is total: two entries differing only in
    // case must not be free to swap between refreshes.
    return a.name < b.name ? -1 : (a.name == b.name ? 0 : 1);
}

/// Human-order comparison: case-insensitive, and numbers compare as numbers so
/// "img2" precedes "img10".
bool lessByName(const FolderEntry &a, const FolderEntry &b)
{
    return compareName(a, b) < 0;
}

/// Newest first, which is what every date order is actually asked for: the
/// question is "what changed recently", not "what is oldest".
bool lessByDate(const QDateTime &a, const QDateTime &b, const FolderEntry &ea, const FolderEntry &eb)
{
    // An invalid date sorts last rather than arbitrarily. A filesystem that
    // records no birth time would otherwise shuffle the whole folder.
    if (a.isValid() != b.isValid()) {
        return a.isValid();
    }
    if (a != b) {
        return a > b;
    }
    // Every order breaks ties by name. Two files written in the same second are
    // common — a build output directory is nothing else — and without this they
    // would reorder between refreshes for no reason the user can see.
    return compareName(ea, eb) < 0;
}

/// Folders first, then by type, then by name. What "kind" means to someone
/// looking for the images among the documents.
bool lessByKind(const FolderEntry &a, const FolderEntry &b)
{
    if (a.isDir != b.isDir) {
        return a.isDir;
    }
    if (a.mimeType != b.mimeType) {
        return a.mimeType < b.mimeType;
    }
    return compareName(a, b) < 0;
}

void sortEntries(std::vector<FolderEntry> &entries, StackSortOrder order)
{
    switch (order) {
    case StackSortOrder::Name:
        std::sort(entries.begin(), entries.end(), lessByName);
        return;
    case StackSortOrder::DateAdded:
        std::sort(entries.begin(), entries.end(), [](const FolderEntry &a, const FolderEntry &b) {
            return lessByDate(a.dateAdded, b.dateAdded, a, b);
        });
        return;
    case StackSortOrder::DateModified:
        std::sort(entries.begin(), entries.end(), [](const FolderEntry &a, const FolderEntry &b) {
            return lessByDate(a.dateModified, b.dateModified, a, b);
        });
        return;
    case StackSortOrder::DateCreated:
        std::sort(entries.begin(), entries.end(), [](const FolderEntry &a, const FolderEntry &b) {
            return lessByDate(a.dateCreated, b.dateCreated, a, b);
        });
        return;
    case StackSortOrder::Kind:
        std::sort(entries.begin(), entries.end(), lessByKind);
        return;
    }
    // No default branch: adding an order must not compile until it is sorted.
}

}

StackModel::StackModel(IFolderBackend *backend, QObject *parent)
    : QAbstractListModel(parent)
    , m_backend(backend)
{
    Q_ASSERT(m_backend);
    m_backend->setChangeCallback([this] {
        refresh();
    });
}

int StackModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant StackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const FolderEntry &entry = m_entries[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name;
    case PathRole:
        return entry.path;
    case IconNameRole:
        return entry.iconName;
    case MimeTypeRole:
        return entry.mimeType;
    case IsDirRole:
        return entry.isDir;
    case SizeRole:
        return entry.size;
    case DateAddedRole:
        return entry.dateAdded;
    case DateModifiedRole:
        return entry.dateModified;
    case DateCreatedRole:
        return entry.dateCreated;
    default:
        return {};
    }
}

QHash<int, QByteArray> StackModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IconNameRole, "iconName"},
        {MimeTypeRole, "mimeType"},
        {IsDirRole, "isDir"},
        {SizeRole, "size"},
        {DateAddedRole, "dateAdded"},
        {DateModifiedRole, "dateModified"},
        {DateCreatedRole, "dateCreated"},
    };
}

QString StackModel::path() const
{
    return m_backend->watchedPath();
}

void StackModel::setPath(const QString &path)
{
    if (path == m_backend->watchedPath()) {
        return;
    }
    m_backend->watch(path);
    Q_EMIT pathChanged();
    refresh();
}

QString StackModel::rootPath() const
{
    return m_rootPath;
}

void StackModel::setRootPath(const QString &path)
{
    const QString normalised = path.isEmpty() ? QString() : QDir::cleanPath(QDir(path).absolutePath());
    if (normalised != m_rootPath) {
        m_rootPath = normalised;
        Q_EMIT rootPathChanged();
    }
    setPath(m_rootPath);
}

bool StackModel::canGoUp() const
{
    const QString current = path();
    return !m_rootPath.isEmpty() && !current.isEmpty() && current != m_rootPath;
}

QVariantList StackModel::trail() const
{
    QVariantList crumbs;
    if (m_rootPath.isEmpty()) {
        return crumbs;
    }

    // Built from the current path back to the root, then reversed: the walk has
    // to start where it knows the answer, and the root is the only place it does.
    QStringList paths;
    for (QString at = path(); !at.isEmpty(); at = QFileInfo(at).path()) {
        paths.prepend(at);
        if (at == m_rootPath) {
            break;
        }
        if (QFileInfo(at).path() == at) {
            // Reached the filesystem root without meeting m_rootPath. The
            // current path is not under the root, so there is no trail to show.
            return {};
        }
    }

    for (const QString &at : paths) {
        QVariantMap crumb;
        const QString name = QFileInfo(at).fileName();
        crumb.insert(QStringLiteral("name"), name.isEmpty() ? at : name);
        crumb.insert(QStringLiteral("path"), at);
        crumbs.append(crumb);
    }
    return crumbs;
}

bool StackModel::enterFolder(int row)
{
    if (row < 0 || row >= rowCount() || !m_entries[row].isDir) {
        return false;
    }
    setPath(m_entries[row].path);
    return true;
}

bool StackModel::goUp()
{
    if (!canGoUp()) {
        return false;
    }
    setPath(QFileInfo(path()).path());
    return true;
}

void StackModel::resetToRoot()
{
    setPath(m_rootPath);
}

int StackModel::sortOrder() const
{
    return static_cast<int>(m_sortOrder);
}

void StackModel::setSortOrder(int order)
{
    if (order < int(StackSortOrder::Name) || order > int(StackSortOrder::Kind)) {
        return;
    }
    const auto next = static_cast<StackSortOrder>(order);
    if (next == m_sortOrder) {
        return;
    }

    m_sortOrder = next;
    Q_EMIT sortOrderChanged();
    // Re-sorts and diffs, so the view sees moves rather than a reset.
    refresh();
}

int StackModel::status() const
{
    return static_cast<int>(m_status);
}

int StackModel::count() const
{
    return rowCount();
}

const FolderEntry &StackModel::entryAt(int row) const
{
    Q_ASSERT(row >= 0 && row < rowCount());
    return m_entries[row];
}

void StackModel::refresh()
{
    const FolderStatus status = m_backend->status();
    if (status != m_status) {
        m_status = status;
        Q_EMIT statusChanged();
    }

    std::vector<FolderEntry> next = m_backend->entries();
    sortEntries(next, m_sortOrder);

    // Walk both lists in step. Where paths agree the row survives and only its
    // data may have changed; where they disagree, the old path either still
    // exists somewhere in the new list — so a row was inserted here — or it does
    // not, so this row was removed. The set of surviving paths answers that in
    // constant time, which matters: a stack pointed at an application directory
    // holds thousands of rows and a nested scan would be quadratic in them.
    QSet<QString> nextPaths;
    nextPaths.reserve(static_cast<qsizetype>(next.size()));
    for (const FolderEntry &entry : next) {
        nextPaths.insert(entry.path);
    }

    const int before = rowCount();
    int i = 0;
    while (true) {
        const int oldCount = static_cast<int>(m_entries.size());
        const int newCount = static_cast<int>(next.size());

        if (i >= oldCount && i >= newCount) {
            break;
        }

        if (i >= newCount) {
            beginRemoveRows(QModelIndex(), i, oldCount - 1);
            m_entries.erase(m_entries.begin() + i, m_entries.end());
            endRemoveRows();
            break;
        }

        if (i >= oldCount) {
            beginInsertRows(QModelIndex(), i, newCount - 1);
            m_entries.insert(m_entries.end(), next.begin() + i, next.end());
            endInsertRows();
            break;
        }

        if (m_entries[i].path == next[i].path) {
            if (m_entries[i] != next[i]) {
                m_entries[i] = next[i];
                const QModelIndex idx = index(i, 0);
                Q_EMIT dataChanged(idx, idx);
            }
            ++i;
            continue;
        }

        if (nextPaths.contains(m_entries[i].path)) {
            beginInsertRows(QModelIndex(), i, i);
            m_entries.insert(m_entries.begin() + i, next[i]);
            endInsertRows();
            ++i;
        } else {
            beginRemoveRows(QModelIndex(), i, i);
            m_entries.erase(m_entries.begin() + i);
            endRemoveRows();
        }
    }

    if (rowCount() != before) {
        Q_EMIT countChanged();
    }
}

}
