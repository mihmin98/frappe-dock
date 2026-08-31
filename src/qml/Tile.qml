import QtQuick
import org.kde.frappedock

/*
 * One cell of the dock.
 *
 * Dimensions come from Dock.qml's proportion bindings; nothing here is a
 * literal except the separator's stroke width, which is 1 pt and deliberately
 * does not scale (plan.md Part 0).
 */
Item {
    id: tile

    /// S, the layout cell edge. Passed down rather than read from config so the
    /// proportion model has exactly one origin.
    required property real iconSize
    required property real gap
    required property bool horizontal
    /// One of ConfigFacade.Bottom / Left / Right, for the indicator's edge.
    required property int dockPosition

    required property string entryId
    required property string entryName
    required property string entryIcon
    required property int entryKind

    property bool isRunning: false
    property int windowCount: 0

    /// Emitted on click, carrying the tile's id — never its index, which shifts
    /// under reordering — and the button and modifiers the binding matrix needs.
    signal activated(string tileId, int button, int modifiers)

    /// Press and hold, which is its own row of the matrix.
    signal held(string tileId)

    /// Whether this tile may be picked up and moved. A separator is a rule
    /// rather than a cell and has nothing to reorder.
    readonly property bool draggable: !isSeparator

    /// Drag gesture, reported in *scene* coordinates. The tile knows where the
    /// pointer is and nothing else — which row it should land on is a question
    /// about the strip, and only the dock can answer it.
    signal dragStarted(point scenePosition)
    signal dragMoved(point scenePosition)
    signal dragFinished()

    /// The DockController, for the two questions a file drop asks: may these
    /// files be opened with this application, and please open them. Null in
    /// tests that do not care, in which case nothing is accepted — refusing is
    /// the safe answer when there is nobody to ask.
    property var controller: null

    /// True while this tile is the one a spring-load is counting down on. Set
    /// by the dock, which owns the countdown; the tile only draws it.
    property bool springArmed: false

    /// A drag has come to rest over this tile, left it, or ended on it. The
    /// dock owns the timer, so the tile only reports what the pointer did.
    signal dragHovered(string tileId)
    signal dragHoverEnded()
    signal dragReleased()

    /// While a drag is over this tile: "none", "accepted" or "rejected".
    readonly property string dropState: !dropTarget.containsDrag ? "none"
                                        : (dropTarget.verdict && dropTarget.verdict.accepted
                                           ? "accepted" : "rejected")

    /// Why the drag hovering over this tile would be refused, in words, or the
    /// empty string when it would not be. Shown by the dock rather than here:
    /// a tile is one cell wide and a sentence is not.
    readonly property string dropRefusal: dropState !== "rejected" ? ""
                                          : tile.refusalText(dropTarget.verdict)

    /// Emitted when the refusal the tile is showing changes, so the dock can
    /// put the message somewhere it fits.
    signal dropFeedbackChanged(string refusal)
    onDropRefusalChanged: tile.dropFeedbackChanged(dropRefusal)

    function refusalText(verdict) {
        if (!verdict) {
            return "";
        }
        if (verdict.rejection === DropRejection.UnsupportedType) {
            return verdict.detail.length > 0
                ? qsTr("%1 can't open %2").arg(verdict.appName).arg(verdict.detail)
                : qsTr("%1 can't open this").arg(verdict.appName);
        }
        return "";
    }

    readonly property bool isSeparator: entryKind === TileKind.Separator

    // A separator is a hairline, not a tile: taller than the icon art it sits
    // between, centred on the shelf rather than aligned to the icons, and given
    // much wider clearance (0.44 S each side against the normal S/3). The Grid
    // already contributes `gap` on each side, so the cell only carries the
    // difference.
    readonly property real separatorClearance: 0.44 * iconSize
    readonly property real separatorCellSize: 1 + 2 * (separatorClearance - gap)

    implicitWidth: isSeparator ? (horizontal ? separatorCellSize : iconSize) : iconSize
    implicitHeight: isSeparator ? (horizontal ? iconSize : separatorCellSize) : iconSize

    Rectangle {
        objectName: "separator"
        visible: tile.isSeparator
        anchors.centerIn: parent

        // 1 pt wide x 1.25 S long. The width is the one dimension in this file
        // that must not scale with S.
        width: tile.horizontal ? 1 : 1.25 * tile.iconSize
        height: tile.horizontal ? 1.25 * tile.iconSize : 1

        color: Qt.rgba(1, 1, 1, 0.3)
    }

    /*
     * File drops.
     *
     * The enter is always accepted, even when the files cannot be opened. A
     * DropArea that refuses the enter stops receiving the drag altogether, and
     * with it any chance of saying *why* — which is precisely the silent
     * rejection §4.4 calls a decades-old papercut. The refusal happens at the
     * drop instead, after the reason has been on screen the whole time.
     */
    DropArea {
        id: dropTarget
        objectName: "dropTarget"

        anchors.fill: parent
        // Only an application can open a file. A separator has nothing behind
        // it, and folder tiles get their own handling in Phase 5.
        enabled: tile.entryKind === TileKind.Application
        keys: ["text/uri-list"]

        /// The controller's answer for the drag currently overhead, or null.
        property var verdict: null

        onEntered: (drag) => {
            // A desktop entry dropped on the dock is an application being
            // added, not a document being opened, whichever tile it happens to
            // land over. Declining the enter passes the drag to the strip
            // underneath, which is the one that knows about regions.
            if (tile.controller
                && tile.controller.evaluateRegionDrop(TileRegion.Pinned, drag.urls).itemKind
                   === DropItemKind.Application) {
                drag.accepted = false;
                return;
            }

            dropTarget.verdict = tile.controller
                ? tile.controller.evaluateDrop(tile.entryId, drag.urls) : null;
            drag.accepted = true;

            // Only a tile that would take the drop springs open. Activating one
            // that has just refused puts two answers on a single gesture, and
            // the refusal takes longer to read than the countdown takes to run,
            // so the launch arrives looking like the refusal was ignored.
            if (dropTarget.verdict && dropTarget.verdict.accepted) {
                tile.dragHovered(tile.entryId);
            }
        }

        onExited: {
            dropTarget.verdict = null;
            tile.dragHoverEnded();
        }

        onDropped: (drop) => {
            let accepted = tile.controller
                && tile.controller.openDroppedFiles(tile.entryId, drop.urls);
            drop.accepted = accepted;
            dropTarget.verdict = null;
            tile.dragReleased();
        }
    }

    /// Drop feedback, behind the artwork: a lift for a drop that will open, a
    /// refusal tint for one that will not. Both are shapes rather than words —
    /// the sentence goes where there is room for it — but the two states are
    /// never the same shape, so the answer is legible without reading.
    Rectangle {
        objectName: "dropHighlight"
        visible: tile.dropState !== "none"

        anchors.centerIn: parent
        width: tile.iconSize + tile.gap
        height: width
        radius: 0.28 * height

        color: tile.dropState === "accepted" ? Qt.rgba(1, 1, 1, 0.25)
                                             : Qt.rgba(0.8, 0.1, 0.1, 0.25)
        border.width: 1
        border.color: tile.dropState === "accepted" ? Qt.rgba(1, 1, 1, 0.5)
                                                    : Qt.rgba(1, 0.3, 0.3, 0.8)
    }

    /*
     * The spring-load countdown.
     *
     * A ring that closes in on the tile over exactly the configured delay, so
     * the wait is visible and its end is predictable — a tile that springs open
     * with no warning reads as the dock doing something of its own accord.
     */
    Rectangle {
        objectName: "springIndicator"
        visible: tile.springArmed

        anchors.centerIn: parent
        width: tile.iconSize + tile.gap
        height: width
        radius: 0.28 * height

        color: "transparent"
        border.width: 2
        border.color: Qt.rgba(1, 1, 1, 0.8)

        // Restarts with each arming, and runs for the delay itself rather than
        // a fraction of it: the ring reaching the tile *is* the spring-load.
        scale: 1.6
        opacity: 0
        NumberAnimation on scale {
            running: tile.springArmed && FrappeConfig.springLoadDelay > 0
            from: 1.6
            to: 1
            duration: FrappeConfig.springLoadDelay
        }
        NumberAnimation on opacity {
            running: tile.springArmed && FrappeConfig.springLoadDelay > 0
            from: 0
            to: 1
            duration: FrappeConfig.springLoadDelay
        }
    }

    Image {
        id: art
        objectName: "art"
        visible: !tile.isSeparator

        anchors.centerIn: parent
        width: tile.iconSize
        height: tile.iconSize

        // Ask for artwork at the size it is drawn at rather than scaling a
        // cached bitmap up.
        sourceSize: Qt.size(tile.iconSize, tile.iconSize)
        source: tile.entryIcon.length > 0 ? "image://frappeicon/" + tile.entryIcon : ""

        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Indicator {
        objectName: "indicator"
        // FrappeConfig gates it globally; isRunning gates it per tile. A
        // separator has no application behind it and so never carries one.
        visible: tile.isRunning && !tile.isSeparator && FrappeConfig.showRunningIndicators

        iconSize: tile.iconSize
        art: art
        dockPosition: tile.dockPosition
        windowCount: tile.windowCount
    }

    // MouseArea rather than TapHandler: the matrix is keyed on modifiers, and
    // TapHandler's tapped() reports the button but not the modifier state.
    MouseArea {
        id: input
        objectName: "input"
        anchors.fill: parent
        enabled: !tile.isSeparator
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

        /// Where the press landed, in scene coordinates: the threshold has to
        /// be measured against a point that does not move when the strip
        /// reflows underneath the pointer.
        property point pressScene
        /// True once the press has travelled far enough to be a drag. It stays
        /// true until the next press, which is what suppresses the click and
        /// the hold that would otherwise follow the release.
        property bool dragging: false

        onPressed: (mouse) => {
            input.dragging = false;
            input.pressScene = input.mapToItem(null, mouse.x, mouse.y);
        }

        onPositionChanged: (mouse) => {
            if (!tile.draggable || !(mouse.buttons & Qt.LeftButton)) {
                return;
            }

            let scene = input.mapToItem(null, mouse.x, mouse.y);
            if (!input.dragging) {
                let travelled = Math.hypot(scene.x - input.pressScene.x,
                                           scene.y - input.pressScene.y);
                if (travelled < Qt.styleHints.startDragDistance) {
                    return;
                }
                input.dragging = true;
                tile.dragStarted(scene);
            }
            tile.dragMoved(scene);
        }

        onReleased: {
            if (input.dragging) {
                tile.dragFinished();
            }
        }

        onCanceled: {
            if (input.dragging) {
                input.dragging = false;
                tile.dragFinished();
            }
        }

        onClicked: (mouse) => {
            if (!input.dragging) {
                tile.activated(tile.entryId, mouse.button, mouse.modifiers);
            }
        }
        onPressAndHold: (mouse) => {
            if (!input.dragging) {
                tile.held(tile.entryId);
            }
        }
    }
}
