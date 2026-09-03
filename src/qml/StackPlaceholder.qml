import QtQuick
import org.kde.frappedock

/*
 * What a stack view shows when it has no rows to show.
 *
 * Three distinct states, not one: a folder that is still being read, a folder
 * that has gone away, and a folder that is genuinely empty are three different
 * facts, and collapsing them into one blank rectangle makes the first two look
 * like the third.
 */
Text {
    id: placeholder

    /// S, for the type size.
    property real cellSize: FrappeConfig.tileSize

    property bool loading: false
    property bool failed: false
    property bool empty: false

    horizontalAlignment: Text.AlignHCenter
    font.pixelSize: cellSize / 3.5
    color: palette.windowText
    opacity: 0.7
    wrapMode: Text.Wrap
    visible: loading || failed || empty

    text: loading ? qsTr("Loading…")
          : failed ? qsTr("This folder is no longer available.")
          : qsTr("Empty folder")
}
