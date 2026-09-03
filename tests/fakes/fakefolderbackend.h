#pragma once

#include "core/interfaces/ifolderbackend.h"

namespace frappe
{

/// In-memory IFolderBackend with a settable listing.
///
/// A real implementation of the interface rather than a generated mock, for the
/// same reason FakeTaskBackend is one: the interface is four calls, and hand
/// writing it is shorter than configuring a framework.
///
/// Nothing here happens on its own. A test that wants the listing to change
/// changes it and calls notifyChange(), which is what the filesystem does —
/// asynchronously, and pretending otherwise would hide ordering bugs.
class FakeFolderBackend : public IFolderBackend
{
public:
    void watch(const QString &path) override
    {
        m_path = path;
        m_entries.clear();
        m_status = path.isEmpty() ? FolderStatus::Idle : FolderStatus::Loading;
    }

    QString watchedPath() const override
    {
        return m_path;
    }

    std::vector<FolderEntry> entries() const override
    {
        return m_entries;
    }

    FolderStatus status() const override
    {
        return m_status;
    }

    void setChangeCallback(std::function<void()> cb) override
    {
        m_callback = std::move(cb);
    }

    /// Replaces the listing and marks it complete. Does not notify.
    void setEntries(std::vector<FolderEntry> entries)
    {
        m_entries = std::move(entries);
        m_status = FolderStatus::Ready;
    }

    void addEntry(const FolderEntry &entry)
    {
        m_entries.push_back(entry);
        m_status = FolderStatus::Ready;
    }

    /// Adds a plain file with no timestamps. Tests that care about dates set
    /// them through addEntry() instead.
    void addFile(const QString &name, const QString &mimeType = QStringLiteral("text/plain"))
    {
        FolderEntry entry;
        entry.name = name;
        entry.path = m_path + u'/' + name;
        entry.mimeType = mimeType;
        entry.iconName = QStringLiteral("text-x-generic");
        addEntry(entry);
    }

    void addDir(const QString &name)
    {
        FolderEntry entry;
        entry.name = name;
        entry.path = m_path + u'/' + name;
        entry.mimeType = QStringLiteral("inode/directory");
        entry.iconName = QStringLiteral("folder");
        entry.isDir = true;
        addEntry(entry);
    }

    void removeEntry(const QString &name)
    {
        std::erase_if(m_entries, [&name](const FolderEntry &e) {
            return e.name == name;
        });
    }

    void setStatus(FolderStatus status)
    {
        m_status = status;
        if (status == FolderStatus::Failed) {
            m_entries.clear();
        }
    }

    /// Pretend the listing changed, the way the filesystem would.
    void notifyChange() const
    {
        if (m_callback) {
            m_callback();
        }
    }

private:
    QString m_path;
    std::vector<FolderEntry> m_entries;
    FolderStatus m_status = FolderStatus::Idle;
    std::function<void()> m_callback;
};

}
