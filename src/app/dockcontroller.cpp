#include "app/dockcontroller.h"

#include "core/interfaces/ilauncherbackend.h"
#include "core/interfaces/itaskbackend.h"
#include "core/model/tilemodel.h"

#include "core/config/configfacade.h"

#include <QLoggingCategory>
#include <QVariantMap>

using namespace frappe;

Q_LOGGING_CATEGORY(FRAPPE_DOCK, "frappe.dock")

DockController::DockController(TileModel *model, const ILauncherBackend *launcher, ITaskBackend *tasks, QObject *parent)
    : QObject(parent)
    , m_model(model)
    , m_launcher(launcher)
    , m_tasks(tasks)
{
}

void DockController::activateTile(const QString &tileId)
{
    if (tileId.isEmpty()) {
        return;
    }

    const auto result = m_launcher->launch(tileId);
    if (!result) {
        qCWarning(FRAPPE_DOCK) << "Could not launch" << tileId << "error" << static_cast<int>(result.error());
    }
}

void DockController::tileClicked(const QString &tileId, int button, int modifiers)
{
    if (tileId.isEmpty()) {
        return;
    }

    // QML hands these over as ints; the matrix speaks Qt's enums.
    route(input::dispatch(static_cast<Qt::MouseButton>(button),
                          static_cast<Qt::KeyboardModifiers>(modifiers)),
          tileId);
}

void DockController::tileHeld(const QString &tileId)
{
    if (tileId.isEmpty()) {
        return;
    }

    route(input::dispatch(Qt::LeftButton, Qt::NoModifier, input::Trigger::PressAndHold), tileId);
}

void DockController::windowsChanged()
{
    if (!m_tasks) {
        return;
    }

    const std::vector<WindowInfo> windows = m_tasks->windows();

    QString active;
    bool stillRunning = false;
    for (const WindowInfo &window : windows) {
        if (active.isEmpty() && window.isActive && !window.appId.isEmpty()) {
            active = window.appId;
        }
        if (window.appId == m_previousActiveAppId) {
            stillRunning = true;
        }
    }

    if (!active.isEmpty()) {
        m_previousActiveAppId = active;
        return;
    }

    // Nothing is active: focus is on the desktop, or on a window with no app id.
    // Keep whatever was in front — clicking the wallpaper does not mean the user
    // has stopped having an application to go back to — unless that application
    // has actually quit, in which case there is nothing left to hide.
    if (!stillRunning) {
        m_previousActiveAppId.clear();
    }
}

int DockController::commandFor(int button, int modifiers, bool held) const
{
    return static_cast<int>(input::dispatch(static_cast<Qt::MouseButton>(button),
                                            static_cast<Qt::KeyboardModifiers>(modifiers),
                                            held ? input::Trigger::PressAndHold : input::Trigger::Click));
}

std::vector<WindowInfo> DockController::windowsOf(const QString &appId) const
{
    std::vector<WindowInfo> result;
    if (!m_tasks) {
        return result;
    }
    for (const WindowInfo &window : m_tasks->windows()) {
        if (window.appId == appId) {
            result.push_back(window);
        }
    }
    return result;
}

QVariantList DockController::contextMenuFor(const QString &tileId) const
{
    // A minimized-window tile stands for a window, not an application, so none
    // of the menu's rows — pin, open at login, quit the app — are about it. An
    // empty list means no popup rather than an empty one.
    if (const Tile *tile = tileFor(tileId); tile && tile->kind != TileKind::Application) {
        return {};
    }

    MenuContext context;
    context.tileId = tileId;
    context.appName = tileId;
    context.windows = windowsOf(tileId);
    context.launchesAtLogin = m_launcher->launchesAtLogin(tileId);

    if (const auto entry = m_launcher->lookup(tileId)) {
        context.appName = entry->name;
        context.actions = entry->actions;
    }

    if (const Tile *tile = tileFor(tileId)) {
        context.isPinned = tile->isPinned;
    }

    // No backend reports responsiveness yet, so Force Quit stays out of the menu
    // — see the notes on 2.5.1. The model supports it; nothing can ask for it.
    context.isResponsive = true;

    QVariantList rows;
    for (const MenuItem &item : buildContextMenu(context)) {
        rows.append(QVariantMap{
            {QStringLiteral("kind"), static_cast<int>(item.kind)},
            {QStringLiteral("itemId"), item.id},
            {QStringLiteral("label"), item.label},
            {QStringLiteral("checkable"), item.checkable},
            {QStringLiteral("checked"), item.checked},
        });
    }
    return rows;
}

void DockController::menuItemTriggered(const QString &tileId, int kind, const QString &itemId)
{
    if (tileId.isEmpty()) {
        return;
    }

    switch (static_cast<MenuItemKind>(kind)) {
    case MenuItemKind::Separator:
        return;

    case MenuItemKind::Window:
        if (m_tasks) {
            m_tasks->activate(itemId);
        }
        return;

    case MenuItemKind::Action: {
        const auto result = m_launcher->launchAction(tileId, itemId);
        if (!result) {
            qCWarning(FRAPPE_DOCK) << "Could not run action" << itemId << "of" << tileId;
        }
        return;
    }

    case MenuItemKind::Pin:
    case MenuItemKind::Unpin: {
        ConfigFacade *config = ConfigFacade::instance();
        QStringList pinned = config->pinnedEntries();
        if (static_cast<MenuItemKind>(kind) == MenuItemKind::Pin) {
            if (!pinned.contains(tileId)) {
                pinned.append(tileId);
            }
        } else {
            pinned.removeAll(tileId);
        }
        config->setPinnedEntries(pinned);
        if (m_model) {
            m_model->rebuild();
        }
        return;
    }

    case MenuItemKind::LaunchAtLogin: {
        // A toggle, so the new state is the opposite of the current one rather
        // than something the caller has to work out and pass in.
        const bool enabled = !m_launcher->launchesAtLogin(tileId);
        const auto result = m_launcher->setLaunchAtLogin(tileId, enabled);
        if (!result) {
            qCWarning(FRAPPE_DOCK) << "Could not change login startup for" << tileId;
        }
        return;
    }

    case MenuItemKind::ShowInFileManager:
        route(input::Command::RevealInFileManager, tileId);
        return;

    case MenuItemKind::Quit:
    case MenuItemKind::ForceQuit:
        // Closing every window is what "quit" means to the window manager; there
        // is no separate quit request in the protocol.
        for (const WindowInfo &window : windowsOf(tileId)) {
            m_tasks->close(window.windowId);
        }
        return;
    }
}

void DockController::route(input::Command command, const QString &tileId)
{
    // No default branch: adding a Command must not compile until it is routed.
    switch (command) {
    case input::Command::LaunchOrActivate:
        launchOrActivate(tileId);
        return;

    case input::Command::RevealInFileManager: {
        const auto result = m_launcher->reveal(tileId);
        if (!result) {
            qCWarning(FRAPPE_DOCK) << "Could not reveal" << tileId << "error" << static_cast<int>(result.error());
        }
        return;
    }

    case input::Command::ActivateAndHidePrevious: {
        // Order matters: hide first, or the app being hidden may be the one that
        // just gained focus.
        const QString previous = m_previousActiveAppId;
        if (!previous.isEmpty() && previous != tileId) {
            hideApp(previous);
        }
        launchOrActivate(tileId);
        return;
    }

    case input::Command::ActivateAndHideOthers:
        if (m_tasks) {
            m_tasks->hideOthers(tileId);
        }
        launchOrActivate(tileId);
        return;

    case input::Command::NewInstance:
        // Launching an application that is already running starts another copy;
        // there is nothing extra to ask for.
        activateTile(tileId);
        return;

    case input::Command::ShowContextMenu:
        Q_EMIT contextMenuRequested(tileId);
        return;

    case input::Command::ShowJumpList:
        Q_EMIT jumpListRequested(tileId);
        return;
    }
}

void DockController::launchOrActivate(const QString &tileId)
{
    // A minimized-window tile stands for one window, and its id is that window's
    // — not an application's. Clicking it restores exactly that window.
    if (const Tile *tile = tileFor(tileId); tile && tile->kind == TileKind::MinimizedWindow) {
        if (m_tasks) {
            m_tasks->activate(tile->id);
        }
        return;
    }

    if (!activateWindows(tileId)) {
        activateTile(tileId);
    }
}

const Tile *DockController::tileFor(const QString &tileId) const
{
    if (!m_model) {
        return nullptr;
    }
    for (int row = 0; row < m_model->rowCount(); ++row) {
        if (m_model->tileAt(row).id == tileId) {
            return &m_model->tileAt(row);
        }
    }
    return nullptr;
}

bool DockController::activateWindows(const QString &appId)
{
    if (!m_tasks) {
        return false;
    }

    const std::vector<WindowInfo> windows = m_tasks->windows();

    // Raise the first window of this app that is not already the active one, so
    // that clicking a tile repeatedly cycles through its windows rather than
    // re-raising the same one. Falls back to the first window when there is only
    // one, or when none is active.
    const WindowInfo *target = nullptr;
    for (const WindowInfo &window : windows) {
        if (window.appId != appId) {
            continue;
        }
        if (!target) {
            target = &window;
        }
        if (!window.isActive) {
            target = &window;
            break;
        }
    }

    if (!target) {
        return false;
    }

    m_tasks->activate(target->windowId);
    return true;
}

void DockController::hideApp(const QString &appId)
{
    if (!m_tasks) {
        return;
    }

    for (const WindowInfo &window : m_tasks->windows()) {
        if (window.appId == appId && !window.isMinimized) {
            m_tasks->minimize(window.windowId);
        }
    }
}
