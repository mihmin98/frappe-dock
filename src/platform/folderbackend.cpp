#include "platform/folderbackend.h"

#include <KCoreDirLister>
#include <KFileItem>

#include <QDir>
#include <QFileInfo>
#include <QUrl>

namespace frappe
{

namespace
{

FolderEntry toEntry(const KFileItem &item)
{
    FolderEntry entry;
    entry.name = item.name();
    entry.path = item.localPath();
    entry.iconName = item.iconName();
    entry.mimeType = item.mimetype();
    entry.isDir = item.isDir();
    entry.size = static_cast<qint64>(item.size());

    // Timestamps come from QFileInfo rather than KFileItem::time(): the two
    // orders the sort menu distinguishes — when an entry appeared in this
    // directory, and when the file itself came into being — are ctime and birth
    // time, and KFileItem exposes neither separately.
    const QFileInfo info(entry.path);
    entry.dateAdded = info.metadataChangeTime();
    entry.dateModified = info.lastModified();
    entry.dateCreated = info.birthTime();
    if (!entry.dateCreated.isValid()) {
        // No birth time on this filesystem. ctime is the nearest honest answer;
        // an invalid date would sort arbitrarily.
        entry.dateCreated = entry.dateAdded;
    }
    return entry;
}

}

FolderBackend::FolderBackend(QObject *parent)
    : QObject(parent)
    , m_lister(std::make_unique<KCoreDirLister>())
{
    m_lister->setAutoUpdate(true);
    m_lister->setDelayedMimeTypes(false);

    const auto onListingChanged = [this] {
        reload();
    };

    connect(m_lister.get(), &KCoreDirLister::newItems, this, onListingChanged);
    connect(m_lister.get(), &KCoreDirLister::itemsDeleted, this, onListingChanged);
    connect(m_lister.get(), &KCoreDirLister::refreshItems, this, onListingChanged);
    connect(m_lister.get(), &KCoreDirLister::clear, this, onListingChanged);
    connect(m_lister.get(), &KCoreDirLister::completed, this, [this] {
        reload();
        setStatus(FolderStatus::Ready);
        notify();
    });
    connect(m_lister.get(), &KCoreDirLister::jobError, this, [this] {
        m_entries.clear();
        setStatus(FolderStatus::Failed);
        notify();
    });

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        if (path != m_path) {
            return;
        }
        if (!QFileInfo(m_path).isDir()) {
            reload(); // the directory itself went away; reload() reports that
            return;
        }
        m_lister->updateDirectory(QUrl::fromLocalFile(m_path));
    });
}

FolderBackend::~FolderBackend()
{
    // ~KCoreDirLister emits clear() on its way out. Without this the handler
    // runs during our own destruction, after the members it reads have already
    // gone — and the lister outlives them, because it is declared first.
    m_lister->disconnect(this);
}

void FolderBackend::watch(const QString &path)
{
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }

    // Absolute, so the path the watcher reports back can be compared to it.
    m_path = path.isEmpty() ? QString() : QDir(path).absolutePath();
    m_entries.clear();

    if (m_path.isEmpty()) {
        m_lister->stop();
        setStatus(FolderStatus::Idle);
        notify();
        return;
    }

    setStatus(FolderStatus::Loading);
    notify();
    m_watcher.addPath(m_path);
    m_lister->openUrl(QUrl::fromLocalFile(m_path));
}

QString FolderBackend::watchedPath() const
{
    return m_path;
}

std::vector<FolderEntry> FolderBackend::entries() const
{
    return m_entries;
}

FolderStatus FolderBackend::status() const
{
    return m_status;
}

void FolderBackend::setChangeCallback(std::function<void()> cb)
{
    m_callback = std::move(cb);
}

void FolderBackend::reload()
{
    // A directory deleted out from under the lister empties the listing without
    // being an error, which is indistinguishable from an empty directory unless
    // we look. Checking here rather than trusting a signal is what makes the
    // vanishing case degrade instead of silently showing nothing.
    if (!m_path.isEmpty() && !QFileInfo(m_path).isDir()) {
        m_entries.clear();
        setStatus(FolderStatus::Failed);
        notify();
        return;
    }

    const KFileItemList items = m_lister->items();
    m_entries.clear();
    m_entries.reserve(static_cast<size_t>(items.size()));
    for (const KFileItem &item : items) {
        if (item.localPath().isEmpty()) {
            continue; // not a local file; a dock stack has nothing to do with it
        }
        m_entries.push_back(toEntry(item));
    }
    notify();
}

void FolderBackend::setStatus(FolderStatus status)
{
    m_status = status;
}

void FolderBackend::notify() const
{
    if (m_callback) {
        m_callback();
    }
}

}
