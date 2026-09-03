#pragma once

#include <QAbstractListModel>
#include <QString>

#include <vector>

#include "core/interfaces/ifolderbackend.h"
#include "core/model/stacksettings.h"

namespace frappe
{

/// The contents of one folder tile, as a list the stack views render from.
///
/// Content is derived, never mutated in place: refresh() re-queries the backend,
/// sorts, diffs against the current rows and emits the model signals the
/// difference implies — the same discipline TileModel uses, for the same reason.
/// A stack can hold thousands of rows, so the diff is linear rather than the
/// nested scan that is fine at dock scale.
///
/// Navigating into a subfolder is just setPath() on the child, and back out is
/// setPath() on the parent. There is no navigation stack because a directory
/// tree already is one.
class StackModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    /// The folder the stack is *for*. Drilling down moves path; rootPath stays,
    /// and is the floor goUp() will not go below — a stack is a view of one
    /// folder, not a file manager that can wander off into the filesystem.
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY pathChanged)
    /// Root to current folder, as [{name, path}, ...], for the breadcrumb.
    Q_PROPERTY(QVariantList trail READ trail NOTIFY pathChanged)
    /// A StackSortOrder. The container sets it from StackSettings; the model
    /// only applies it, so the order is testable without a config file.
    Q_PROPERTY(int sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IconNameRole,
        MimeTypeRole,
        IsDirRole,
        SizeRole,
        DateAddedRole,
        DateModifiedRole,
        DateCreatedRole,
    };
    Q_ENUM(Roles)

    /// Mirrors FolderStatus, so QML need not include the backend interface.
    enum Status { Idle, Loading, Ready, Failed };
    Q_ENUM(Status)

    explicit StackModel(IFolderBackend *backend, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const;
    void setPath(const QString &path);

    QString rootPath() const;
    /// Sets the folder the stack is for and navigates to it.
    void setRootPath(const QString &path);

    bool canGoUp() const;
    QVariantList trail() const;

    /// Navigates into the folder at \a row. Returns false when the row is not a
    /// folder, which is the view's cue to open the file instead.
    Q_INVOKABLE bool enterFolder(int row);

    /// Navigates to the parent folder. Returns false at the root.
    Q_INVOKABLE bool goUp();

    /// Navigates back to the root. What closing and reopening a stack does, so
    /// it does not reopen three folders deep into wherever it was left.
    Q_INVOKABLE void resetToRoot();

    int sortOrder() const;
    void setSortOrder(int order);

    int status() const;
    int count() const;

    /// The entry at \a row, for tests and for callers that want the whole
    /// struct. The row must exist.
    const FolderEntry &entryAt(int row) const;

Q_SIGNALS:
    void pathChanged();
    void sortOrderChanged();
    void rootPathChanged();
    void statusChanged();
    void countChanged();

private:
    /// Re-queries the backend and reconciles the rows with what it reports.
    void refresh();

    IFolderBackend *m_backend;
    std::vector<FolderEntry> m_entries;
    FolderStatus m_status = FolderStatus::Idle;
    QString m_rootPath;
    StackSortOrder m_sortOrder = StackSortOrder::Name;
};

}
