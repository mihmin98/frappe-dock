#include "platform/taskbackend.h"

#include <QUrl> // the taskmanager headers only forward-declare it

#include <abstracttasksmodel.h>
#include <tasksmodel.h>

using namespace frappe;
using namespace Qt::StringLiterals;

namespace
{
/// The roles live in a class, not a namespace — an alias keeps call sites short.
using Role = TaskManager::AbstractTasksModel;

bool isWindowRow(const QModelIndex &index)
{
    return index.data(Role::IsWindow).toBool();
}
}

TaskBackend::TaskBackend(QObject *parent)
    : QObject(parent)
    , m_model(new TaskManager::TasksModel(this))
{
    // The dock does its own grouping and its own filtering; it wants every
    // window on the session, ungrouped, and decides what to show itself.
    m_model->setFilterByVirtualDesktop(false);
    m_model->setFilterByScreen(false);
    m_model->setFilterByActivity(false);
    m_model->setGroupMode(TaskManager::TasksModel::GroupDisabled);

    // QQmlParserStatus hooks: QML calls these, C++ has to do it by hand.
    m_model->classBegin();
    m_model->componentComplete();

    // Re-query and diff rather than tracking deltas (plan.md §2.4), so every
    // change signal collapses into the same notification.
    const auto notify = [this] {
        notifyChanged();
    };
    connect(m_model, &QAbstractItemModel::rowsInserted, this, notify);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, notify);
    connect(m_model, &QAbstractItemModel::rowsMoved, this, notify);
    connect(m_model, &QAbstractItemModel::dataChanged, this, notify);
    connect(m_model, &QAbstractItemModel::modelReset, this, notify);
    connect(m_model, &QAbstractItemModel::layoutChanged, this, notify);
}

TaskBackend::~TaskBackend() = default;

QString TaskBackend::idForRow(const QModelIndex &index) const
{
    const QPersistentModelIndex persistent(index);
    for (auto it = m_ids.cbegin(); it != m_ids.cend(); ++it) {
        if (it.value() == persistent) {
            return it.key();
        }
    }

    const QString id = u"w%1"_s.arg(m_nextId++);
    m_ids.insert(id, persistent);
    return id;
}

QModelIndex TaskBackend::indexForId(const QString &windowId) const
{
    const QPersistentModelIndex persistent = m_ids.value(windowId);
    return persistent.isValid() ? QModelIndex(persistent) : QModelIndex();
}

void TaskBackend::notifyChanged()
{
    // Drop ids whose window has gone, so the map does not grow for the lifetime
    // of the session.
    for (auto it = m_ids.begin(); it != m_ids.end();) {
        it = it.value().isValid() ? std::next(it) : m_ids.erase(it);
    }

    if (m_callback) {
        m_callback();
    }
}

std::vector<WindowInfo> TaskBackend::windows() const
{
    std::vector<WindowInfo> result;
    const int rows = m_model->rowCount();
    result.reserve(static_cast<size_t>(rows));

    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_model->index(row, 0);
        if (!isWindowRow(index)) {
            // Launcher and startup rows are the launcher model's business.
            continue;
        }

        WindowInfo info;
        info.windowId = idForRow(index);
        info.appId = index.data(Role::AppId).toString();
        info.title = index.data(Qt::DisplayRole).toString(); // there is no Title role
        info.isMinimized = index.data(Role::IsMinimized).toBool();
        info.isActive = index.data(Role::IsActive).toBool();
        result.push_back(std::move(info));
    }

    return result;
}

void TaskBackend::activate(const QString &windowId)
{
    const QModelIndex index = indexForId(windowId);
    if (index.isValid()) {
        m_model->requestActivate(index);
    }
}

void TaskBackend::minimize(const QString &windowId)
{
    const QModelIndex index = indexForId(windowId);
    // The model only offers a toggle, so guard on the current state to keep
    // minimize() idempotent the way the interface implies.
    if (index.isValid() && !index.data(Role::IsMinimized).toBool()) {
        m_model->requestToggleMinimized(index);
    }
}

void TaskBackend::close(const QString &windowId)
{
    const QModelIndex index = indexForId(windowId);
    if (index.isValid()) {
        m_model->requestClose(index);
    }
}

void TaskBackend::hideOthers(const QString &exceptAppId)
{
    // Collect first: minimizing can reorder the model, and mutating it while
    // walking rows by number would skip windows.
    QList<QPersistentModelIndex> targets;
    const int rows = m_model->rowCount();
    for (int row = 0; row < rows; ++row) {
        const QModelIndex index = m_model->index(row, 0);
        if (!isWindowRow(index)) {
            continue;
        }
        if (index.data(Role::AppId).toString() == exceptAppId) {
            continue;
        }
        if (index.data(Role::IsMinimized).toBool()) {
            continue;
        }
        targets.append(QPersistentModelIndex(index));
    }

    for (const QPersistentModelIndex &target : std::as_const(targets)) {
        if (target.isValid()) {
            m_model->requestToggleMinimized(QModelIndex(target));
        }
    }
}

void TaskBackend::setChangeCallback(std::function<void()> cb)
{
    m_callback = std::move(cb);
}
