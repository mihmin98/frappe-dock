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

    required property string entryId
    required property string entryName
    required property string entryIcon
    required property int entryKind

    property bool isRunning: false

    /// Emitted on click, carrying the tile's id — never its index, which shifts
    /// under reordering.
    signal activated(string tileId)

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
        visible: tile.isRunning && !tile.isSeparator

        iconSize: tile.iconSize
        // Centred 0.21 S below the artwork's bottom edge, which places it inside
        // the shelf's bottom padding band.
        anchors.horizontalCenter: art.horizontalCenter
        anchors.top: art.bottom
        anchors.topMargin: 0.21 * tile.iconSize - height / 2
    }

    TapHandler {
        enabled: !tile.isSeparator
        onTapped: tile.activated(tile.entryId)
    }
}
