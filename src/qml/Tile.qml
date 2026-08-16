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
        anchors.fill: parent
        enabled: !tile.isSeparator
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton

        onClicked: (mouse) => tile.activated(tile.entryId, mouse.button, mouse.modifiers)
        onPressAndHold: (mouse) => tile.held(tile.entryId)
    }
}
