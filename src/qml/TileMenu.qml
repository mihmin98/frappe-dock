import QtQuick
import QtQuick.Controls
import org.kde.frappedock

/*
 * The tile context menu and jump list — the same popup either way, because they
 * are the same list assembled by the same function; press-and-hold simply opens
 * it without the pointer having to reach the right button.
 *
 * Contents come from C++: this file decides how a row looks, never what rows
 * there are. Chrome is left to the Controls style, per the "we build behaviour,
 * not appearance" rule.
 */
Menu {
    id: menu

    // Its own surface, not an item inside the dock's. The dock masks its input
    // region down to the shelf so the magnification and drag headroom above it
    // do not swallow clicks meant for the desktop — and an in-window menu opens
    // into exactly that headroom, where it is drawn but cannot be clicked. A
    // window popup is a child surface the compositor places and routes input to
    // itself, so the mask does not apply to it.
    popupType: Popup.Window

    /// The DockController. Supplies the rows and carries out the choice.
    required property var controller

    /// The tile the menu was opened for.
    property string tileId: ""

    /// Which edge the dock is on, so the menu opens away from it.
    property int dockPosition: ConfigFacade.Bottom

    /// Rebuilds for \a id and opens against \a anchor, the tile's own item.
    ///
    /// Rebuilt on every open rather than bound: window lists and pinned state
    /// both change while the dock is up, and a menu built once would show
    /// whichever of them was true the first time.
    function openFor(id, anchor) {
        menu.tileId = id;

        // Menu has no clear(); the rows from the last open have to be taken back
        // out one at a time, and destroyed, or they leak for the session.
        while (menu.count > 0) {
            menu.takeItem(0).destroy();
        }

        let rows = menu.controller ? menu.controller.contextMenuFor(id) : [];
        if (rows.length === 0) {
            // Nothing to show. An empty popup is worse than none: it is a
            // rectangle the user has to dismiss to find out it was empty.
            return;
        }

        for (let row of rows) {
            // Two different Controls types, chosen per row, which is why this is
            // built rather than repeated over: a Repeater delegate is one type.
            let component = row.kind === MenuItemKind.Separator ? separatorComponent : itemComponent;
            // Created against the content item, not the Menu: a Menu is not an
            // Item, and parenting to it warns that the object never reached the
            // scene even though addItem() then places it.
            menu.addItem(component.createObject(menu.contentItem, { row: row }));
        }
        if (!anchor) {
            // No tile to place against — a test, or a row that has gone away
            // mid-gesture. The pointer is the next best origin.
            menu.popup();
            return;
        }

        // Placed against the tile and opening away from the screen edge the dock
        // is on, so the menu never covers the tile it is about. The compositor
        // constrains it to the output from there.
        switch (menu.dockPosition) {
        case ConfigFacade.Left:
            menu.popup(anchor, anchor.width, 0);
            break;
        case ConfigFacade.Right:
            menu.popup(anchor, -menu.width, 0);
            break;
        case ConfigFacade.Bottom:
        default:
            menu.popup(anchor, 0, -menu.height);
            break;
        }
    }

    Component {
        id: separatorComponent

        MenuSeparator {
            required property var row
        }
    }

    Component {
        id: itemComponent

        MenuItem {
            required property var row

            text: row.label
            checkable: row.checkable
            checked: row.checked

            onTriggered: menu.controller.menuItemTriggered(menu.tileId, row.kind, row.itemId)
        }
    }
}
