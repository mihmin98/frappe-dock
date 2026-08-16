#pragma once

#include <QString>

#include <utility>
#include <vector>

#include "core/interfaces/itaskbackend.h"

namespace frappe
{

/// What a menu row does. The renderer switches on this rather than on the
/// label, so nothing user-visible is load-bearing.
enum class MenuItemKind {
    Separator,
    /// Raise one specific window. `id` is its window id.
    Window,
    /// An action the desktop entry declares. `id` is the action's id.
    Action,
    Pin,
    Unpin,
    LaunchAtLogin,
    ShowInFileManager,
    Quit,
    ForceQuit,
};

struct MenuItem {
    MenuItemKind kind = MenuItemKind::Separator;
    /// Window id or action id, depending on kind; empty for the fixed rows.
    QString id;
    QString label;
    /// Rendered as a checkbox when the row is checkable — currently only
    /// LaunchAtLogin.
    bool checkable = false;
    bool checked = false;

    bool operator==(const MenuItem &) const = default;
};

/// Everything the menu depends on, gathered by the caller.
///
/// A plain struct of already-resolved facts rather than a set of backend
/// pointers: it makes the assembly a pure function, and the sixteen-odd
/// combinations of pinned x running x window count x autostart become a table of
/// test inputs instead of a set of mocks.
struct MenuContext {
    /// The tile's id, which is also the desktop entry id.
    QString tileId;
    QString appName;
    /// The desktop entry's declared actions, in its own order: (id, label).
    std::vector<std::pair<QString, QString>> actions;
    /// This application's windows, in the order the backend reported them.
    std::vector<WindowInfo> windows;
    bool isPinned = false;
    bool launchesAtLogin = false;
    /// False when the application has stopped answering the compositor. Only
    /// then is Force Quit offered.
    bool isResponsive = true;
};

/// Assembles the context menu.
///
/// Pure: same context in, same rows out. Separators appear only between
/// non-empty groups — never leading, trailing, or doubled — so the caller does
/// not have to think about them.
std::vector<MenuItem> buildContextMenu(const MenuContext &context);

}
