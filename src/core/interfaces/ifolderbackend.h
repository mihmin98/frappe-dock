#pragma once

#include <QDateTime>
#include <QString>

#include <functional>
#include <vector>

namespace frappe
{

/// One entry of a watched directory, as the dock cares about it.
///
/// Timestamps are carried on the entry rather than fetched on demand because
/// sorting touches every row at once; a stat per comparison would turn an
/// O(n log n) sort into O(n log n) syscalls.
struct FolderEntry {
    QString name;
    QString path; ///< absolute local path
    QString iconName;
    QString mimeType;
    bool isDir = false;
    qint64 size = 0;

    /// Inode change time. The closest thing a POSIX filesystem has to "when did
    /// this appear here": it moves when a file is created, linked or moved into
    /// the directory, which is what the sort order is asking about.
    QDateTime dateAdded;
    QDateTime dateModified;
    /// Birth time where the filesystem records one, dateAdded where it does not.
    QDateTime dateCreated;

    bool operator==(const FolderEntry &) const = default;
};

/// How far along the current listing is. A stack has to draw something while a
/// large directory is still being read, and has to draw something else when the
/// directory it was watching has been deleted out from under it.
enum class FolderStatus {
    Idle, ///< nothing watched yet
    Loading,
    Ready,
    Failed, ///< unreadable, or vanished
};

/// Directory listing and change notification.
///
/// The containment wall around KIO, for the same reason ITaskBackend is one
/// around the window-management library: core sees only this, platform
/// implements it, tests fake it. Plain virtual class — no Q_OBJECT, no signals
/// — so core stays free of moc and, more to the point, free of the Qt GUI
/// dependency every KIO library drags in.
class IFolderBackend
{
public:
    virtual ~IFolderBackend() = default;

    /// Lists \a path and watches it for changes, replacing any previous watch.
    /// Listing is asynchronous: status() is Loading until it finishes.
    virtual void watch(const QString &path) = 0;

    /// The path passed to the last watch(), empty before the first.
    virtual QString watchedPath() const = 0;

    /// The contents of the watched directory, in no particular order. Empty
    /// while loading and after a failure.
    virtual std::vector<FolderEntry> entries() const = 0;

    virtual FolderStatus status() const = 0;

    /// Invoked whenever the listing or its status changes in any way. Core
    /// re-queries and diffs rather than tracking deltas.
    virtual void setChangeCallback(std::function<void()> cb) = 0;
};

}
