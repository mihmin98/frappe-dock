pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * A stack shown as a fan: entries laid along an arc rising from the tile.
 *
 * The fan is the mode that trades capacity for recognisability — a dozen items
 * you can hit without reading are worth more than two hundred you have to. It
 * therefore caps rather than scrolls: past `maxItems` the remaining entries are
 * reported through `overflowCount` so the container can offer the grid instead,
 * which is the mode that does stay usable at that size.
 *
 * The arc's radius grows with the number of items rather than the sweep angle
 * widening, so a fan of four and a fan of ten lean the same way and only differ
 * in reach. A widening sweep would put the last item somewhere different every
 * time a file was added to the folder.
 *
 * Every dimension derives from `cellSize`, S in the proportion model. Angles are
 * not dimensions and are written as angles.
 */
Item {
    id: fan

    /// The StackModel. Null in tests that only care about layout.
    property var model: null

    /// S, the layout cell edge.
    property real cellSize: FrappeConfig.tileSize
    readonly property real gap: cellSize * GeometryTuning.spacingRatio
    readonly property real cellPitch: cellSize + gap

    /// Which edge the dock is on, so the fan rises away from it.
    property int dockPosition: ConfigFacade.Bottom

    /// How many entries the fan will draw. Beyond this it stops rather than
    /// crowding: an arc has a finite amount of room along it.
    property int maxItems: 10

    /// How wide a name may run beside its icon before it elides, in cell
    /// widths. Fixed rather than fitted to the longest name, for the reason the
    /// list view gives: a fan that resized itself to its contents would change
    /// shape every time a file landed in the folder.
    property real labelWidths: 3

    /// How far the fan sweeps, in radians. An angle, not a length, so it does
    /// not scale with S — the fan leans the same amount at every dock size.
    readonly property real sweep: Math.PI / 2.4

    signal fileActivated(string path)

    readonly property int rowsInModel: model ? model.count : 0
    readonly property int shownCount: Math.min(rowsInModel, maxItems)
    /// Entries the fan is not showing. Zero when it is showing all of them.
    readonly property int overflowCount: Math.max(0, rowsInModel - maxItems)

    /// Radius of the arc the entries sit on. Chosen so adjacent entries are one
    /// cell pitch apart along it, with a floor so a fan of two is still a fan
    /// rather than two items on top of each other.
    readonly property real arcRadius: Math.max(cellPitch * 1.5, shownCount * cellPitch / sweep)

    /// Angle between adjacent entries: one cell pitch of arc at that radius.
    readonly property real angleStep: cellPitch / arcRadius

    readonly property bool loading: model ? model.status === StackModel.Loading : false
    readonly property bool failed: model ? model.status === StackModel.Failed : false
    readonly property bool empty: model ? (model.status === StackModel.Ready && model.count === 0) : false

    /// How far the arc reaches from its origin, along each axis. The first
    /// entry sits straight up at the full radius; the last leans out by the
    /// sine of the sweep.
    readonly property real arcSpanX: arcRadius * Math.sin(sweep)
    readonly property real arcSpanY: arcRadius

    /// Room beside an icon for its name.
    readonly property real labelWidth: labelWidths * cellSize

    // Reach, plus a cell of artwork at the far end of it, plus that entry's
    // label, plus the header.
    implicitWidth: arcSpanX + cellSize + gap + labelWidth + 2 * gap
    implicitHeight: header.height + arcSpanY + cellSize + 3 * gap

    StackHeader {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: fan.gap
        height: fan.cellPitch - fan.gap

        model: fan.model
        cellSize: fan.cellSize
    }

    Item {
        id: arc

        objectName: "stackFanArc"

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: fan.gap

        /// Where the fan springs from: the corner nearest the tile. For a bottom
        /// dock that is the bottom-left, and the arc rises to the right.
        readonly property real originX: fan.dockPosition === ConfigFacade.Right
                                        ? width - fan.cellSize / 2 : fan.cellSize / 2
        readonly property real originY: height - fan.cellSize / 2
        /// Which way the arc sweeps out of the origin. A dock on the right edge
        /// has to fan the other way or the entries leave the screen.
        readonly property int direction: fan.dockPosition === ConfigFacade.Right ? -1 : 1

        Repeater {
            id: entries

            model: fan.loading || !fan.model ? null : fan.model

            delegate: MouseArea {
                id: entry

                required property int index
                required property string name
                required property string path
                required property string iconName
                required property bool isDir

                // Past the cap there is no room on the arc; the container offers
                // the grid instead, via overflowCount.
                visible: entry.index < fan.maxItems
                enabled: visible

                // The artwork plus its name, so the name is part of the target
                // rather than a decoration beside it.
                width: fan.cellSize + fan.gap + fan.labelWidth
                height: fan.cellSize

                /// Angle from straight up, growing away from the screen edge.
                /// Entry 0 therefore sits directly above the origin, which is
                /// where the eye starts.
                readonly property real angle: entry.index * fan.angleStep

                /// Where on the arc this entry's *artwork* is centred. The
                /// label hangs off the side and must not drag the icon off the
                /// curve, so the arc is defined by the icon and nothing else.
                readonly property real arcX: arc.originX + arc.direction * fan.arcRadius * Math.sin(angle)
                readonly property real arcY: arc.originY - fan.arcRadius * Math.cos(angle)

                /// The label sits on the side the fan sweeps towards, so it
                /// leans away from the screen edge with everything else.
                readonly property bool labelTrails: arc.direction > 0
                readonly property real iconX: labelTrails ? 0 : entry.width - fan.cellSize

                x: entry.arcX - entry.iconX - fan.cellSize / 2
                y: entry.arcY - height / 2

                onClicked: {
                    if (!fan.model.enterFolder(entry.index)) {
                        fan.fileActivated(entry.path);
                    }
                }

                Image {
                    x: entry.iconX
                    width: fan.cellSize
                    height: fan.cellSize
                    source: entry.iconName.length > 0 ? "image://frappeicon/" + entry.iconName + IconTreatment.token : ""
                    sourceSize: Qt.size(fan.cellSize, fan.cellSize)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Text {
                    x: entry.labelTrails ? fan.cellSize + fan.gap : 0
                    width: fan.labelWidth
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.name
                    font.pixelSize: fan.cellSize / 3
                    color: DockPalette.text
                    horizontalAlignment: entry.labelTrails ? Text.AlignLeft : Text.AlignRight
                    // Middle, not right: file names in a folder often share a
                    // prefix and differ at the end, which is the part elision
                    // from the right would remove.
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    StackPlaceholder {
        anchors.centerIn: arc
        width: arc.width
        cellSize: fan.cellSize
        loading: fan.loading
        failed: fan.failed
        empty: fan.empty
    }
}
