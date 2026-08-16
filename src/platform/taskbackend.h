#pragma once

#include <QHash>
#include <QObject>
#include <QPersistentModelIndex>

#include "core/interfaces/itaskbackend.h"

namespace TaskManager
{
class TasksModel;
}

namespace frappe
{

/// ITaskBackend over the Plasma window-management model.
///
/// This is the only translation unit in the project that may include a
/// libtaskmanager header; everything else sees ITaskBackend. QObject only so it
/// can hold connections to the model's change signals.
///
/// Requires the compositor to allow org_kde_plasma_window_management for this
/// binary — see docs/decisions/2026-08-16-libtaskmanager-api-surface.md. Without
/// that permission the model stays empty and silent.
class TaskBackend : public QObject, public ITaskBackend
{
    Q_OBJECT

public:
    explicit TaskBackend(QObject *parent = nullptr);
    ~TaskBackend() override;

    std::vector<WindowInfo> windows() const override;

    void activate(const QString &windowId) override;
    void minimize(const QString &windowId) override;
    void close(const QString &windowId) override;
    void hideOthers(const QString &exceptAppId) override;

    void setChangeCallback(std::function<void()> cb) override;

private:
    /// Row → the stable id the dock hands out, minting one on first sight.
    QString idForRow(const QModelIndex &index) const;
    QModelIndex indexForId(const QString &windowId) const;
    void notifyChanged();

    TaskManager::TasksModel *m_model;
    std::function<void()> m_callback;

    /// The model's own row numbers are not stable across insertions, and Wayland
    /// window ids are not meaningful outside the owning process, so the dock
    /// mints its own and anchors them to persistent indices.
    mutable QHash<QString, QPersistentModelIndex> m_ids;
    mutable quint64 m_nextId = 1;
};

}
