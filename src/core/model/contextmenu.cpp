#include "core/model/contextmenu.h"

#include "core/model/stacksettings.h"

#include <iterator>

using namespace frappe;
using namespace Qt::StringLiterals;

namespace
{
MenuItem simple(MenuItemKind kind, const QString &label)
{
    MenuItem item;
    item.kind = kind;
    item.label = label;
    return item;
}

/// Appends \a group, preceded by a separator if there is already something to
/// separate it from. Keeping this in one place is what makes the "no leading,
/// trailing or doubled separators" rule true by construction rather than by
/// careful reading.
void appendGroup(std::vector<MenuItem> &items, const std::vector<MenuItem> &group)
{
    if (group.empty()) {
        return;
    }
    if (!items.empty()) {
        items.push_back(MenuItem{});
    }
    items.insert(items.end(), group.begin(), group.end());
}
}

std::vector<MenuItem> frappe::buildContextMenu(const MenuContext &context)
{
    std::vector<MenuItem> items;

    // Windows, but only when there is a choice to make. One window is already
    // what clicking the tile does, so listing it is a row that teaches nothing.
    if (context.windows.size() > 1) {
        std::vector<MenuItem> group;
        group.reserve(context.windows.size());
        for (const WindowInfo &window : context.windows) {
            MenuItem item;
            item.kind = MenuItemKind::Window;
            item.id = window.windowId;
            // A window with no title still needs a row that can be told apart
            // from its siblings.
            item.label = window.title.isEmpty() ? context.appName : window.title;
            group.push_back(std::move(item));
        }
        appendGroup(items, group);
    }

    // The application's own actions, in the order its desktop entry declares
    // them — that order is the application author's decision, not ours.
    std::vector<MenuItem> actions;
    actions.reserve(context.actions.size());
    for (const auto &[id, label] : context.actions) {
        MenuItem item;
        item.kind = MenuItemKind::Action;
        item.id = id;
        item.label = label;
        actions.push_back(std::move(item));
    }
    appendGroup(items, actions);

    // Options: what the dock can do with the application, as opposed to what the
    // application can do.
    std::vector<MenuItem> options;
    options.push_back(context.isPinned ? simple(MenuItemKind::Unpin, u"Remove from Dock"_s)
                                       : simple(MenuItemKind::Pin, u"Keep in Dock"_s));

    MenuItem atLogin = simple(MenuItemKind::LaunchAtLogin, u"Open at Login"_s);
    atLogin.checkable = true;
    atLogin.checked = context.launchesAtLogin;
    options.push_back(std::move(atLogin));

    options.push_back(simple(MenuItemKind::ShowInFileManager, u"Show in File Manager"_s));
    appendGroup(items, options);

    // Quitting, which only means anything while the application is running.
    if (!context.windows.empty()) {
        std::vector<MenuItem> quitting;
        quitting.push_back(simple(MenuItemKind::Quit, u"Quit"_s));
        if (!context.isResponsive) {
            // Offered only when the ordinary request has nothing to answer it,
            // so that the destructive option is not the one sitting under the
            // pointer every time.
            quitting.push_back(simple(MenuItemKind::ForceQuit, u"Force Quit"_s));
        }
        appendGroup(items, quitting);
    }

    return items;
}

std::vector<MenuItem> frappe::buildStackMenu(const StackMenuContext &context)
{
    std::vector<MenuItem> items;

    // The three modes as checkable rows, one of them checked, rather than a
    // single row that cycles: a cycling row cannot say what the other options
    // are, and §5.3 asks the menu to show the current mode.
    const std::pair<StackViewMode, QString> modes[] = {
        {StackViewMode::Grid, u"Grid"_s},
        {StackViewMode::Fan, u"Fan"_s},
        {StackViewMode::List, u"List"_s},
    };

    std::vector<MenuItem> view;
    view.reserve(std::size(modes));
    for (const auto &[mode, label] : modes) {
        MenuItem item;
        item.kind = MenuItemKind::StackView;
        item.id = QString::number(static_cast<int>(mode));
        item.label = label;
        item.checkable = true;
        item.checked = context.viewMode == static_cast<int>(mode);
        view.push_back(std::move(item));
    }
    appendGroup(items, view);

    // Sorting, as its own group. Five rows beside three would read as one list
    // of eight unrelated choices without the rule between them.
    const std::pair<StackSortOrder, QString> orders[] = {
        {StackSortOrder::Name, u"Sort by Name"_s},
        {StackSortOrder::DateAdded, u"Sort by Date Added"_s},
        {StackSortOrder::DateModified, u"Sort by Date Modified"_s},
        {StackSortOrder::DateCreated, u"Sort by Date Created"_s},
        {StackSortOrder::Kind, u"Sort by Kind"_s},
    };

    std::vector<MenuItem> sorting;
    sorting.reserve(std::size(orders));
    for (const auto &[order, label] : orders) {
        MenuItem item;
        item.kind = MenuItemKind::StackSort;
        item.id = QString::number(static_cast<int>(order));
        item.label = label;
        item.checkable = true;
        item.checked = context.sortOrder == static_cast<int>(order);
        sorting.push_back(std::move(item));
    }
    appendGroup(items, sorting);

    std::vector<MenuItem> options;
    options.push_back(simple(MenuItemKind::ShowInFileManager, u"Show in File Manager"_s));
    options.push_back(simple(MenuItemKind::RemoveFolder, u"Remove from Dock"_s));
    appendGroup(items, options);

    return items;
}
