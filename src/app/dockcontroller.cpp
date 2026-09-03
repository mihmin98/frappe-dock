#include "app/dockcontroller.h"

#include "core/interfaces/ilauncherbackend.h"
#include "core/interfaces/itaskbackend.h"
#include "core/model/filedrop.h"
#include "core/model/regiondrop.h"
#include "core/model/stacksettings.h"
#include "core/model/tilemodel.h"

#include <QLoggingCategory>
#include <QVariantMap>

using namespace frappe;

Q_LOGGING_CATEGORY(FRAPPE_DOCK, "frappe.dock")

namespace
{
/// Menu rows as QML wants them. Shared by the application and folder menus so
/// the two cannot describe the same row differently.
QVariantList toRows(const std::vector<MenuItem> &items)
{
    QVariantList rows;
    rows.reserve(static_cast<qsizetype>(items.size()));
    for (const MenuItem &item : items) {
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
}

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
    // A folder tile is about a folder, not an application: how to draw the
    // stack and how to take it off the dock, none of which buildContextMenu()
    // knows anything about.
    if (const Tile *tile = tileFor(tileId); tile && tile->kind == TileKind::Folder) {
        StackMenuContext stack;
        stack.folderPath = tile->id;
        stack.folderName = tile->name;
        stack.viewMode = StackSettings::instance()->viewMode(tile->id);
        stack.sortOrder = StackSettings::instance()->sortOrder(tile->id);
        return toRows(buildStackMenu(stack));
    }

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

    return toRows(buildContextMenu(context));
}

QVariantMap DockController::evaluateDrop(const QString &tileId, const QList<QUrl> &files) const
{
    DropVerdict verdict;
    QString appName = tileId;

    const Tile *tile = tileFor(tileId);
    if (!tile || tile->kind != TileKind::Application) {
        // A separator or a minimized window is not something a file can be
        // opened with. Nothing is declared, so nothing is accepted.
        verdict = frappe::evaluateDrop(QStringList(), files);
    } else if (const auto entry = m_launcher->lookup(tileId)) {
        appName = entry->name;
        verdict = frappe::evaluateDrop(entry->mimeTypes, files);
    } else {
        verdict = frappe::evaluateDrop(QStringList(), files);
    }

    return QVariantMap{
        {QStringLiteral("accepted"), verdict.accepted()},
        {QStringLiteral("rejection"), static_cast<int>(verdict.rejection)},
        {QStringLiteral("detail"), verdict.detail},
        {QStringLiteral("appName"), appName},
    };
}

namespace
{
/// The desktop entry id for a dropped `.desktop` URL. The file name *is* the
/// storage id for anything installed; a lookup decides whether it really is.
QString entryIdFor(const QUrl &url)
{
    return url.fileName();
}
}

QVariantMap DockController::evaluateRegionDrop(int region, const QList<QUrl> &files) const
{
    RegionDropVerdict verdict = frappe::evaluateRegionDrop(files, static_cast<Region>(region));
    QString detail;

    // An application the dock cannot find is a different refusal from an
    // application in the wrong place, and only the launcher can tell them
    // apart. Checked after the region rules so the message names the first
    // thing wrong, not the last.
    if (verdict.accepted() && verdict.kind == DropItemKind::Application) {
        for (const QUrl &file : files) {
            const auto entry = m_launcher->lookup(entryIdFor(file));
            if (!entry) {
                verdict.rejection = RegionDropRejection::NotInstalled;
                detail = file.fileName();
                break;
            }
        }
    }

    return QVariantMap{
        {QStringLiteral("accepted"), verdict.accepted()},
        {QStringLiteral("rejection"), static_cast<int>(verdict.rejection)},
        {QStringLiteral("itemKind"), static_cast<int>(verdict.kind)},
        {QStringLiteral("expectedRegion"), static_cast<int>(verdict.expectedRegion)},
        {QStringLiteral("detail"), detail},
    };
}

bool DockController::acceptRegionDrop(int region, const QList<QUrl> &files)
{
    if (!m_model) {
        return false;
    }
    if (!evaluateRegionDrop(region, files).value(QStringLiteral("accepted")).toBool()) {
        return false;
    }

    const DropItemKind kind = classifyPayload(files);

    if (kind == DropItemKind::File || kind == DropItemKind::Folder) {
        bool addedAny = false;
        for (const QUrl &file : files) {
            // Local paths only. A remote URL has no path for the file region to
            // hold, and evaluateRegionDrop classifies it as Unknown anyway.
            addedAny = m_model->addFileEntry(file.toLocalFile()) || addedAny;
        }
        return addedAny;
    }

    if (kind != DropItemKind::Application) {
        return false;
    }

    bool pinnedAny = false;
    for (const QUrl &file : files) {
        pinnedAny = m_model->setPinned(entryIdFor(file), true) || pinnedAny;
    }
    return pinnedAny;
}

bool DockController::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    const auto result = m_launcher->openUrl(QUrl::fromLocalFile(path));
    if (!result) {
        qCWarning(FRAPPE_DOCK) << "Could not open" << path << "error" << static_cast<int>(result.error());
        return false;
    }
    return true;
}

bool DockController::openDroppedFiles(const QString &tileId, const QList<QUrl> &files)
{
    if (!evaluateDrop(tileId, files).value(QStringLiteral("accepted")).toBool()) {
        return false;
    }

    const auto result = m_launcher->openWith(tileId, files);
    if (!result) {
        qCWarning(FRAPPE_DOCK) << "Could not open" << files.size() << "file(s) with" << tileId
                               << "error" << static_cast<int>(result.error());
        return false;
    }
    return true;
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
        // The model owns pinning: the drag-out gesture in §4.2 writes the same
        // list, and two places that edit it would be two places to keep in step.
        if (m_model) {
            m_model->setPinned(tileId, static_cast<MenuItemKind>(kind) == MenuItemKind::Pin);
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

    case MenuItemKind::StackView:
        // itemId is the mode as a decimal string; the menu carries one kind for
        // all three rows so the set can grow without the switch learning about it.
        StackSettings::instance()->setViewMode(tileId, itemId.toInt());
        return;

    case MenuItemKind::StackSort:
        StackSettings::instance()->setSortOrder(tileId, itemId.toInt());
        return;

    case MenuItemKind::RemoveFolder:
        if (m_model) {
            // Forget the folder's preferences too: a stored view mode for a
            // folder no longer on the dock is a line with nothing to apply it to.
            m_model->removeFileEntry(tileId);
            StackSettings::instance()->forget(tileId);
        }
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
