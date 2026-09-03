pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * A stack shown as a scrollable, drillable grid.
 *
 * This is the mode the reference platform dropped and the one that gets used
 * most: a folder of applications opened here is the closest thing to an
 * application grid the desktop still has, so it has to stay usable at a few
 * hundred entries rather than merely correct at a dozen.
 *
 * Every dimension derives from `cellSize` — S in the proportion model, the same
 * number Dock.qml calls iconSize. A grid at S = 24 is a uniform scale of one at
 * S = 128, so there are no literals below beyond the model's own ratios.
 * Navigation state lives in StackModel; this file decides how a folder looks,
 * never which folder is shown.
 */
Item {
    id: grid

    /// The StackModel. Null in tests that only care about layout, in which case
    /// nothing is shown — there is no folder to show.
    property var model: null

    /// S, the layout cell edge. Passed in rather than read from config so the
    /// proportion model keeps exactly one origin, as in Tile.qml.
    property real cellSize: FrappeConfig.tileSize

    /// S/3, the same constant as the dock's inter-icon gap and shelf padding.
    readonly property real gap: cellSize * GeometryTuning.spacingRatio

    /// How many columns to lay out. The grid grows down, not sideways: a stack
    /// that widened with its contents would be a stack that changes shape every
    /// time a file is added to the folder.
    property int columns: 5

    /// How many rows fit before the grid scrolls instead of growing.
    property int visibleRows: 4

    /// Emitted when a row that is not a folder is chosen. Opening a file is the
    /// application's business, not the view's.
    signal fileActivated(string path)

    readonly property real cellPitch: cellSize + gap
    /// Room under the artwork for a label of two lines.
    readonly property real cellHeight: cellPitch + 2 * labelMetrics.height
    readonly property real headerHeight: cellPitch

    implicitWidth: columns * cellPitch + 2 * gap
    implicitHeight: headerHeight + Math.min(rowsInModel, visibleRows) * cellHeight + 2 * gap

    /// Counted from the model, not from the GridView. The grid's height depends
    /// on this, and the view's height depends on the grid's, so reading the
    /// view's own count here is a binding loop — which Qt breaks by quietly
    /// leaving the height at whatever it was, giving a grid one row tall
    /// however many entries the folder holds.
    readonly property int rowsInModel: {
        let n = model ? model.count : 0;
        return n > 0 ? Math.ceil(n / columns) : 1;
    }

    /// Text metrics for one line at the current scale, so the label box below
    /// each icon is sized from the font rather than from a guessed constant.
    TextMetrics {
        id: labelMetrics
        font.pixelSize: grid.cellSize / 4
        text: "Ag"
    }

    StackHeader {
        id: header

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: grid.gap
        height: grid.headerHeight - grid.gap

        model: grid.model
        cellSize: grid.cellSize
    }

    // --- Contents ---------------------------------------------------------
    GridView {
        id: view

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.margins: grid.gap
        anchors.topMargin: 0

        clip: true
        cellWidth: grid.cellPitch
        cellHeight: grid.cellHeight
        // Reset to the top on every navigation. Carrying the old scroll offset
        // into a folder that may be shorter than it leaves the view parked past
        // the end of the new contents.
        onCountChanged: view.positionViewAtBeginning()

        model: grid.loading || !grid.model ? null : grid.model

        delegate: MouseArea {
            id: entry

            required property int index
            required property string name
            required property string path
            required property string iconName
            required property bool isDir

            width: view.cellWidth
            height: view.cellHeight

            onClicked: {
                // The model answers whether the row was a folder, so the view
                // never has to decide what "open" means for one.
                if (!grid.model.enterFolder(entry.index)) {
                    grid.fileActivated(entry.path);
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: grid.gap / 4

                Image {
                    width: grid.cellSize
                    height: grid.cellSize
                    anchors.horizontalCenter: parent.horizontalCenter
                    // The icon name comes from the backend, which resolves a
                    // folder to a folder icon and a file to its type's — so
                    // folders read as folders without this file deciding it.
                    source: entry.iconName.length > 0 ? "image://frappeicon/" + entry.iconName : ""
                    sourceSize: Qt.size(grid.cellSize, grid.cellSize)
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                Text {
                    width: grid.cellPitch
                    horizontalAlignment: Text.AlignHCenter
                    text: entry.name
                    font.pixelSize: labelMetrics.font.pixelSize
                    color: palette.windowText
                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                }
            }
        }
    }

    // --- States the contents are not ---------------------------------------
    /// True while a listing is still arriving. A folder of a few entries is
    /// listed faster than a frame, so this is only ever seen on a large one —
    /// which is exactly the case where showing nothing at all reads as a bug.
    readonly property bool loading: model ? model.status === StackModel.Loading : false
    readonly property bool failed: model ? model.status === StackModel.Failed : false
    readonly property bool empty: model ? (model.status === StackModel.Ready && model.count === 0) : false

    StackPlaceholder {
        anchors.centerIn: view
        width: view.width
        cellSize: grid.cellSize
        loading: grid.loading
        failed: grid.failed
        empty: grid.empty
    }
}
