#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

#include <memory>

#include "core/interfaces/ifolderbackend.h"

class KCoreDirLister;

namespace frappe
{

/// IFolderBackend over KCoreDirLister.
///
/// KCoreDirLister rather than KDirModel: the latter lives in KIOWidgets and
/// would pull QtWidgets in behind it, and this class already holds the rows in
/// the shape core wants, so a second model underneath would only be a thing to
/// keep in sync.
///
/// Every lister signal lands on the same handler, which re-reads the whole
/// listing and hands it up. Directories change rarely and the model above diffs
/// what it gets, so tracking deltas here would buy nothing and cost the class of
/// bug where an update path is missing.
class FolderBackend : public QObject, public IFolderBackend
{
    Q_OBJECT

public:
    explicit FolderBackend(QObject *parent = nullptr);
    ~FolderBackend() override;

    void watch(const QString &path) override;
    QString watchedPath() const override;
    std::vector<FolderEntry> entries() const override;
    FolderStatus status() const override;
    void setChangeCallback(std::function<void()> cb) override;

private:
    void reload();
    void setStatus(FolderStatus status);
    void notify() const;

    std::unique_ptr<KCoreDirLister> m_lister;
    /// Second line of defence, not a second source of data. KCoreDirLister
    /// drops an update when the change arrives in the window just after one of
    /// its own updates completes — reproducibly, for a deletion following an
    /// addition — and the stack then shows a row for a file that is gone. This
    /// watches the same directory and asks the lister to look again.
    QFileSystemWatcher m_watcher;
    QString m_path;
    std::vector<FolderEntry> m_entries;
    FolderStatus m_status = FolderStatus::Idle;
    std::function<void()> m_callback;
};

}
