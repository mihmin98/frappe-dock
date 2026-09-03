import QtQuick
import org.kde.frappedock

/*
 * The running-application dot.
 *
 * Geometry is measured (plan.md Part 0): 0.085 S across, its centre 0.21 S clear
 * of the artwork's edge. Which edge depends on where the dock is: the outer edge
 * — the one facing the screen border — for a bottom dock, and the inner edge —
 * facing the desktop — for a side dock, so that the dots never sit in the
 * hairline between the shelf and the screen.
 *
 * Placement is computed here rather than by the caller because it is a function
 * of the dock's edge, which the caller would otherwise have to re-derive for
 * every tile.
 */
Item {
    id: indicator

    /// S, the layout cell edge.
    required property real iconSize

    /// The artwork this indicator belongs to. Positioning is relative to it, not
    /// to the cell: an icon theme may inset artwork within the cell, and the dot
    /// must follow the art.
    required property Item art

    /// One of ConfigFacade.Bottom / Left / Right.
    required property int dockPosition

    /// Windows of this application. Only consulted when showWindowCount is set.
    property int windowCount: 1

    /// Draw one dot per window rather than a single dot for "running at all".
    /// Off by default; the setting that turns it on belongs to the appearance
    /// page, not here.
    property bool showWindowCount: false

    readonly property real dotSize: 0.085 * iconSize
    /// Distance from the artwork's edge to the dot's centre.
    readonly property real clearance: 0.21 * iconSize

    readonly property bool horizontal: dockPosition === ConfigFacade.Bottom
    /// Capped: past a few windows the dots stop being countable and start being
    /// a smear, and the count is on the tooltip anyway.
    readonly property int dotCount: showWindowCount ? Math.max(1, Math.min(windowCount, 3)) : 1

    implicitWidth: dots.width
    implicitHeight: dots.height
    width: implicitWidth
    height: implicitHeight

    // Positioned rather than anchored: the anchor set differs per edge, and three
    // conditional anchor bindings are harder to read than the arithmetic they
    // stand for.
    x: {
        if (horizontal)
            return art.x + art.width / 2 - width / 2;
        if (dockPosition === ConfigFacade.Left)
            return art.x + art.width + clearance - width / 2;
        return art.x - clearance - width / 2;
    }
    y: horizontal ? art.y + art.height + clearance - height / 2
                  : art.y + art.height / 2 - height / 2

    Grid {
        id: dots
        objectName: "dots"

        columns: indicator.horizontal ? indicator.dotCount : 1
        rows: indicator.horizontal ? 1 : indicator.dotCount
        spacing: indicator.dotSize

        Repeater {
            model: indicator.dotCount

            Rectangle {
                objectName: "dot"
                width: indicator.dotSize
                height: indicator.dotSize
                radius: width / 2
                color: DockPalette.indicator
            }
        }
    }
}
