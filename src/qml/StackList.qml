pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * A stack shown as a vertical list: one row per entry, artwork at the leading
 * edge, name beside it.
 *
 * The same StackModel the grid and the fan render from, and the same navigation
 * — clicking a folder enters it, and the back button returns. What differs is
 * only the arrangement, which is the point of having three modes rather than
 * three views.
 *
 * Every dimension derives from `cellSize`, S in the proportion model.
 */
Item {
    id: list

    /// The StackModel. Null in tests that only care about layout.
    property var model: null

    /// S, the layout cell edge.
    property real cellSize: FrappeConfig.tileSize

    /// S/3, the dock's inter-icon gap and shelf padding, from the one place
    /// that ratio is written down.
    readonly property real gap: cellSize * GeometryTuning.spacingRatio

    /// How many rows fit before the list scrolls instead of growing.
    property int visibleRows: 8

    /// How wide a row's text may run before it elides, in cell widths. A list
    /// that widened to fit its longest file name would change width every time
    /// the folder's contents changed.
    property real labelWidths: 5

    signal fileActivated(string path)

    /// A row is one cell tall plus the gap either side of it, which is the same
    /// rule the dock's shelf thickness follows.
    readonly property real rowHeight: cellSize + gap
    readonly property real headerHeight: rowHeight

    readonly property int rowsInModel: model ? model.count : 0

    implicitWidth: cellSize + gap * 3 + labelWidths * cellSize
    implicitHeight: headerHeight + Math.max(1, Math.min(rowsInModel, visibleRows)) * rowHeight + gap

    readonly property bool loading: model ? model.status === StackModel.Loading : false
    readonly property bool failed: model ? model.status === StackModel.Failed : false
    readonly property bool empty: model ? (model.status === StackModel.Ready && model.count === 0) : false

    StackHeader {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: list.gap
        height: list.headerHeight - list.gap

        model: list.model
        cellSize: list.cellSize
    }

    ListView {
        id: view

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: list.gap
        anchors.topMargin: 0

        clip: true
        // Reset on every navigation: carrying an offset into a shorter folder
        // parks the view past the end of its contents.
        onCountChanged: view.positionViewAtBeginning()

        model: list.loading || !list.model ? null : list.model

        delegate: MouseArea {
            id: row

            required property int index
            required property string name
            required property string path
            required property string iconName
            required property bool isDir

            width: view.width
            height: list.rowHeight

            onClicked: {
                if (!list.model.enterFolder(row.index)) {
                    list.fileActivated(row.path);
                }
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                spacing: list.gap

                Image {
                    width: list.cellSize
                    height: list.cellSize
                    source: row.iconName.length > 0 ? "image://frappeicon/" + row.iconName + IconTreatment.token : ""
                    sourceSize: Qt.size(list.cellSize, list.cellSize)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: list.labelWidths * list.cellSize
                    text: row.name
                    font.pixelSize: list.cellSize / 3
                    color: DockPalette.text
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    StackPlaceholder {
        anchors.centerIn: view
        width: view.width
        cellSize: list.cellSize
        loading: list.loading
        failed: list.failed
        empty: list.empty
    }
}
