pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * The back button and breadcrumb shared by every stack view.
 *
 * Shared rather than repeated three times: navigation is the same question in
 * Grid, Fan and List, and three copies of it is three places for the answer to
 * drift.
 */
Item {
    id: header

    /// The StackModel being navigated. Null in tests that only care about layout.
    property var model: null

    /// S, the layout cell edge; everything here derives from it.
    property real cellSize: FrappeConfig.tileSize
    readonly property real gap: cellSize * GeometryTuning.spacingRatio

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: header.gap / 2

        Item {
            id: backButton

            objectName: "stackBackButton"
            width: header.cellSize / 2
            height: width
            // Present but inert at the root, rather than appearing and
            // disappearing: a control that comes and goes moves the breadcrumb
            // sideways every time the user drills in.
            opacity: header.model && header.model.canGoUp ? 1 : 0.3

            Image {
                anchors.fill: parent
                source: "image://frappeicon/go-previous"
                sourceSize: Qt.size(width, height)
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            MouseArea {
                anchors.fill: parent
                enabled: header.model ? header.model.canGoUp : false
                onClicked: header.model.goUp()
            }
        }

        Repeater {
            id: breadcrumb

            model: header.model ? header.model.trail : []

            delegate: Row {
                id: crumb

                required property var modelData
                required property int index

                spacing: parent.spacing

                Text {
                    text: "›"
                    color: crumbLabel.color
                    font: crumbLabel.font
                    visible: crumb.index > 0
                }

                Text {
                    id: crumbLabel

                    text: crumb.modelData.name
                    font.pixelSize: header.cellSize / 3.5
                    color: palette.windowText
                    // The last crumb is where you already are.
                    opacity: crumb.index === breadcrumb.count - 1 ? 1 : 0.7

                    MouseArea {
                        anchors.fill: parent
                        enabled: crumb.index < breadcrumb.count - 1
                        onClicked: header.model.path = crumb.modelData.path
                    }
                }
            }
        }
    }
}
