pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * An open stack: the container that picks a view, and the thing that is
 * positioned against the folder tile.
 *
 * It owns none of the arrangement — Grid, Fan and List each do that — and none
 * of the navigation, which is StackModel's. What it owns is which of the three
 * is showing, where the whole thing sits, and when it closes.
 *
 * Position comes from StackAnchor, in C++, from the tile's *resting*
 * centre. That is the fix for the reference platform's §11 defect and the
 * reason `anchorCentre` is what it is; see the decision record.
 */
Item {
    id: popup

    /// The StackModel for the folder being shown. Injected, because the model
    /// needs a folder backend and QML cannot build one.
    property var model: null

    /// The folder tile this belongs to, as its path. Also the key the view mode
    /// and sort order are stored under.
    property string folderPath: ""

    /// S, the layout cell edge.
    property real cellSize: FrappeConfig.tileSize
    readonly property real gap: cellSize * GeometryTuning.spacingRatio

    /// Which edge the dock is on, as a ConfigFacade.Position.
    property int dockPosition: ConfigFacade.Bottom

    /// The parent tile's centre along the dock's axis, in this item's parent's
    /// coordinates, **at rest**.
    ///
    /// Resting, not drawn. Opening a stack happens with the pointer over the
    /// tile, so the tile is magnified and its drawn centre is displaced; the
    /// moment the pointer moves, that centre slides back and an anchored stack
    /// slides with it. That is the reported defect, and binding to the resting
    /// centre is what makes it impossible here.
    property real anchorCentre: 0

    /// The output's size, for keeping the stack on screen.
    property real outputWidth: 0
    property real outputHeight: 0

    /// True while the stack is showing. Setting it false returns the model to
    /// the folder the tile is for, so reopening does not resume three folders
    /// deep into wherever it was left.
    property bool open: false
    onOpenChanged: {
        if (!open && popup.model) {
            popup.model.resetToRoot();
        }
    }

    signal fileActivated(string path)
    signal closeRequested()

    /// Which arrangement is showing, as a StackViewMode.
    ///
    /// Refreshed from StackSettings rather than bound to it. A binding would
    /// have to depend on a plain function call, which has nothing to invalidate
    /// it — and the usual dodge of reading a revision counter for its side
    /// effect is a dead read the QML compiler is free to elide. The signals
    /// below are the invalidation, so they drive it directly.
    property int currentMode: StackViewMode.Grid

    /// Re-reads the stored view mode and sort order for the current folder.
    function refreshPreferences() {
        if (popup.folderPath.length === 0) {
            popup.currentMode = StackViewMode.Grid;
            return;
        }
        popup.currentMode = StackSettings.viewMode(popup.folderPath);
        if (popup.model) {
            popup.model.sortOrder = StackSettings.sortOrder(popup.folderPath);
        }
    }

    onFolderPathChanged: popup.refreshPreferences()
    onModelChanged: popup.refreshPreferences()
    Component.onCompleted: popup.refreshPreferences()

    Connections {
        target: StackSettings

        function onViewModeChanged(folderPath) {
            if (folderPath === popup.folderPath) {
                popup.refreshPreferences();
            }
        }

        function onSortOrderChanged(folderPath) {
            // An empty path means the default moved, which affects every folder
            // that has no preference of its own — including, possibly, this one.
            if (folderPath === popup.folderPath || folderPath.length === 0) {
                popup.refreshPreferences();
            }
        }
    }

    visible: open
    enabled: open

    implicitWidth: content.item ? content.item.implicitWidth : 0
    implicitHeight: content.item ? content.item.implicitHeight : 0
    width: implicitWidth
    height: implicitHeight

    // --- Placement --------------------------------------------------------
    readonly property var placement: StackAnchor.place(popup.dockPosition, popup.anchorCentre,
                                                       popup.cellSize, GeometryTuning.spacingRatio,
                                                       popup.width, popup.height,
                                                       popup.outputWidth, popup.outputHeight)
    x: placement.x
    y: placement.y

    /*
     * Swallows clicks that land on the stack but not on anything in it.
     *
     * Behind the arrangement, so entries, the back button and the breadcrumb all
     * get their clicks first. What it catches is the rest — the padding, the gap
     * between two icons, the empty half of a part-filled last row.
     *
     * Without it those clicks fall through to the dismissal backdrop behind the
     * whole dock, and missing an icon by two pixels closes the stack instead of
     * doing nothing.
     */
    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
    }

    // --- The arrangement --------------------------------------------------
    Loader {
        id: content

        anchors.fill: parent

        sourceComponent: {
            switch (popup.currentMode) {
            case StackViewMode.Fan:
                return fanComponent;
            case StackViewMode.List:
                return listComponent;
            case StackViewMode.Grid:
            default:
                return gridComponent;
            }
        }
    }

    Component {
        id: gridComponent

        StackGrid {
            model: popup.model
            cellSize: popup.cellSize
            onFileActivated: path => popup.fileActivated(path)
        }
    }

    Component {
        id: fanComponent

        StackFan {
            model: popup.model
            cellSize: popup.cellSize
            dockPosition: popup.dockPosition
            onFileActivated: path => popup.fileActivated(path)
        }
    }

    Component {
        id: listComponent

        StackList {
            model: popup.model
            cellSize: popup.cellSize
            onFileActivated: path => popup.fileActivated(path)
        }
    }
}
