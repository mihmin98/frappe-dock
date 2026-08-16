pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * The dock shelf.
 *
 * Every dimension here derives from one number, `tileSize` — S in the
 * proportion model in plan.md Part 0. A dock at S = 24 must be a uniform scale
 * of a dock at S = 128, so there are no numeric literals below beyond the
 * model's own ratios. tst_tile.qml's proportionsFollowTileSize() is what keeps
 * that true.
 *
 * Phase 3 replaces tile *positions* with geometry-engine output. It does not
 * replace these proportions.
 */
Item {
    id: dock

    /// The TileModel, injected by the surface factory (or by a test).
    property var tileModel: null

    /// The DockController, injected the same way. Null in tests that only care
    /// about layout; popups are then simply not offered.
    property var controller: null

    /// Emitted when a tile is activated. main() routes these to the controller,
    /// which dispatches them through the binding matrix.
    signal tileClicked(string tileId, int button, int modifiers)
    signal tileHeld(string tileId)

    // --- The proportion model -------------------------------------------
    readonly property real iconSize: FrappeConfig.tileSize      // S, the layout cell
    readonly property real gap: iconSize / 3                    // S/3
    readonly property real thickness: iconSize + 2 * gap        // 5S/3
    readonly property real shelfRadius: 0.28 * thickness        // 0.28 x thickness
    // --------------------------------------------------------------------

    /// Cell pitch: the distance between the leading edges of two adjacent tiles.
    /// Exposed for tests and for the Phase 3 engine.
    readonly property real cellPitch: iconSize + gap

    readonly property bool horizontal: FrappeConfig.position === ConfigFacade.Bottom

    /// The shelf's own thickness. The gap to the screen edge is a layer-shell
    /// margin on the surface, not padding here, so the shelf fills its window on
    /// the anchored axis.
    implicitWidth: horizontal ? shelf.width : thickness
    implicitHeight: horizontal ? thickness : shelf.height

    /// Routes one interaction: opens a popup here, or hands it to C++.
    function handle(id, button, modifiers, held) {
        let command = dock.controller ? dock.controller.commandFor(button, modifiers, held)
                                      : DockCommand.LaunchOrActivate;

        if (command === DockCommand.ShowContextMenu || command === DockCommand.ShowJumpList) {
            menu.openFor(id);
            return;
        }

        if (held) {
            dock.tileHeld(id);
        } else {
            dock.tileClicked(id, button, modifiers);
        }
    }

    TileMenu {
        id: menu
        objectName: "tileMenu"
        controller: dock.controller
    }

    Rectangle {
        id: shelf
        objectName: "shelf"

        anchors.centerIn: parent

        // Shelf padding equals the inter-icon gap — one constant, used in both
        // places. This is not a coincidence to be re-derived; it falls out of the
        // fit Part 0 chooses.
        width: dock.horizontal ? row.width + 2 * dock.gap : dock.thickness
        height: dock.horizontal ? dock.thickness : row.height + 2 * dock.gap

        radius: dock.shelfRadius

        // Colour and material are Phase 6's business, delegated to the blur
        // effect and the colour scheme. What is normative here is the shape and
        // the rim stroke's existence and 1 pt width, which does not scale.
        color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.2)

        Grid {
            id: row
            objectName: "row"

            // Positioned rather than centred: anchors.centerIn rounds to whole
            // pixels, which at a fractional gap leaves the padding off by up to
            // half a pixel on each side and stops being exactly `gap`.
            x: dock.gap
            y: dock.gap

            columns: dock.horizontal ? repeater.count : 1
            rows: dock.horizontal ? 1 : repeater.count
            spacing: dock.gap

            Repeater {
                id: repeater
                objectName: "repeater"
                model: dock.tileModel

                delegate: Tile {
                    required property string tileId
                    required property string name
                    required property string iconName
                    required property int kind
                    // Tile already declares these two under the same names the
                    // model roles use, so they are marked required rather than
                    // re-declared, and the Repeater fills them directly.
                    required isRunning
                    required windowCount

                    iconSize: dock.iconSize
                    gap: dock.gap
                    horizontal: dock.horizontal
                    dockPosition: FrappeConfig.position

                    entryId: tileId
                    entryName: name
                    entryIcon: iconName
                    entryKind: kind

                    // Popups have to open on the surface that was clicked, so
                    // the view asks the matrix what the input means and keeps
                    // those two commands for itself. Everything else goes to
                    // C++ as before; the matrix is still the only place the
                    // mapping is written down.
                    onActivated: (id, button, modifiers) => dock.handle(id, button, modifiers, false)
                    onHeld: (id) => dock.handle(id, Qt.LeftButton, Qt.NoModifier, true)
                }
            }
        }
    }
}
