#pragma once

#include <QObject>
#include <QVariantList>

#include "core/input/dispatch.h"
#include "core/model/contextmenu.h"
#include "core/model/tile.h"

namespace frappe
{

class ILauncherBackend;
class ITaskBackend;
class TileModel;

/// Turns tile interactions into backend calls.
///
/// The decision of *what* an interaction means belongs to input::dispatch();
/// this class only carries the decision out. Anything it cannot do itself —
/// menus, jump lists — it re-emits for the UI.
class DockController : public QObject
{
    Q_OBJECT

public:
    DockController(TileModel *model, const ILauncherBackend *launcher, ITaskBackend *tasks = nullptr, QObject *parent = nullptr);

public Q_SLOTS:
    /// Launches the application behind \a tileId. Unknown or unlaunchable ids are
    /// logged and ignored: a dock must degrade rather than die.
    void activateTile(const QString &tileId);

    /// The entry point for a real interaction: dispatches \a button and
    /// \a modifiers through the binding matrix and routes the resulting command.
    void tileClicked(const QString &tileId, int button, int modifiers);

    /// Press and hold on \a tileId, which is its own row of the matrix.
    void tileHeld(const QString &tileId);

    /// The Command the matrix produces for this input, as an int QML can
    /// compare against the DockCommand enum. Lets the view decide whether an
    /// interaction opens a popup — which it must handle itself, on the surface
    /// that was clicked — without duplicating the matrix in QML.
    int commandFor(int button, int modifiers, bool held) const;

    /// The rows of the context menu for \a tileId, as a list of maps QML can
    /// repeat over: kind, id, label, checkable, checked.
    ///
    /// Assembled on demand rather than kept as a model: a menu is read once and
    /// thrown away, and anything cached here would be stale by the time the next
    /// right-click arrives.
    QVariantList contextMenuFor(const QString &tileId) const;

    /// Carries out the row the user chose. \a kind is a MenuItemKind and
    /// \a itemId its window or action id, both as they came from
    /// contextMenuFor().
    void menuItemTriggered(const QString &tileId, int kind, const QString &itemId);

    /// Call when the window list changes, so the controller can keep track of
    /// which application was active before the current one.
    ///
    /// The task backend has a single change channel, so whoever owns both this
    /// and the model must call both from it — see main().
    void windowsChanged();

Q_SIGNALS:
    /// The UI owns these two: the controller decides they were asked for, QML
    /// decides where the popup goes.
    void contextMenuRequested(const QString &tileId);
    void jumpListRequested(const QString &tileId);

private:
    void route(input::Command command, const QString &tileId);

    /// Raises the application's windows, or launches it if it has none.
    void launchOrActivate(const QString &tileId);
    /// Activates the application's windows. Returns false if it has none.
    bool activateWindows(const QString &appId);
    void hideApp(const QString &appId);
    /// The tile with this id, or null. Row order shifts, so nothing holds on to
    /// the pointer past the call.
    const Tile *tileFor(const QString &tileId) const;
    /// This application's windows, in backend order.
    std::vector<WindowInfo> windowsOf(const QString &appId) const;

    TileModel *m_model;
    const ILauncherBackend *m_launcher;
    ITaskBackend *m_tasks;

    /// The application that was in front before whatever happens next — i.e.
    /// the currently active one, tracked continuously so that it is already
    /// known by the time a click needs it. State the dock has to keep itself:
    /// no backend reports it, and it must follow every activation, not only the
    /// ones the dock caused.
    QString m_previousActiveAppId;
};

}
