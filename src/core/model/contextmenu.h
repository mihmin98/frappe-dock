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
    /// Draw the stack this way. `id` is the StackViewMode as a decimal string —
    /// one kind rather than three, so the renderer keeps switching on the kind
    /// and the set of modes can grow without the menu learning about it.
    StackView,
    /// Remove the folder tile from the dock. Distinct from Unpin because it
    /// removes a file entry rather than a pinned application, and the two are
    /// separate lists.
    RemoveFolder,
    /// Sort the stack this way. `id` is the StackSortOrder as a decimal string,
    /// for the same reason StackView carries its mode that way.
    StackSort,
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

/// Everything the folder-tile menu depends on. Gathered by the caller, for the
/// same reason MenuContext is: it makes the assembly a pure function.
struct StackMenuContext {
    /// The folder's path, which is also the tile's id.
    QString folderPath;
    QString folderName;
    /// The mode the stack is currently drawn in, as a StackViewMode.
    int viewMode = 0;
    /// The order it is currently listed in, as a StackSortOrder.
    int sortOrder = 0;
};

/// Assembles the folder-tile context menu: how to draw the stack, and how to
/// take it off the dock. Pure, like buildContextMenu().
std::vector<MenuItem> buildStackMenu(const StackMenuContext &context);

/// Assembles the context menu.
///
/// Pure: same context in, same rows out. Separators appear only between
/// non-empty groups — never leading, trailing, or doubled — so the caller does
/// not have to think about them.
std::vector<MenuItem> buildContextMenu(const MenuContext &context);

}
