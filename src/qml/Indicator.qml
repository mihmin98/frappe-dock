import QtQuick

/*
 * The running-application dot.
 *
 * Geometry is measured (plan.md Part 0): 0.085 S across. Its placement relative
 * to the artwork is the caller's business, because only the caller knows where
 * the artwork is. What is set here is the size and the shape.
 *
 * Phase 2 is what makes it appear; the geometry is recorded now because it was
 * measured alongside everything else and would otherwise be re-derived.
 */
Rectangle {
    id: indicator

    /// S, the layout cell edge.
    required property real iconSize

    implicitWidth: 0.085 * iconSize
    implicitHeight: implicitWidth
    width: implicitWidth
    height: implicitHeight

    radius: width / 2
    color: Qt.rgba(1, 1, 1, 0.85)
}
