#pragma once

namespace frappe::geometry
{

/// Which screen edge the dock is anchored to.
///
/// Top has no equivalent in the `position` config key, which offers Bottom,
/// Left and Right only. It is handled here because the arithmetic is the mirror
/// of Bottom and leaving a hole in a four-way switch is how the fourth case
/// becomes wrong later, unnoticed.
enum class DockEdge {
    Bottom,
    Left,
    Right,
    Top,
};

/// Everything placing an open stack needs. Pure input.
///
/// Note what is *not* here: the pointer. A stack's position is not a function of
/// where the pointer is, and the whole of the §11 defect is what happens when it
/// becomes one — see restingTileCentre below.
struct StackAnchorParams {
    DockEdge edge = DockEdge::Bottom;

    /// The parent tile's centre along the dock's axis, **as it rests** — not
    /// where the tile is currently drawn.
    ///
    /// This is the fix for the reference platform's defect. Opening a stack
    /// happens with the pointer over the tile, so the tile is at its magnified
    /// size and its drawn centre is displaced; the moment the pointer moves the
    /// tile shrinks and that centre slides back. Anchoring to it makes the open
    /// stack jump on the first pointer movement, which is exactly the reported
    /// symptom. The resting centre does not move for any pointer position, so a
    /// stack anchored to it cannot.
    double restingTileCentre = 0.0;

    /// Screen edge to the shelf's outer face: the floating gap, g/3.
    double shelfGap = 0.0;
    /// The shelf itself, across the dock's axis.
    double shelfThickness = 0.0;
    /// Between the shelf's inner face and the stack.
    double clearance = 0.0;

    double popupWidth = 0.0;
    double popupHeight = 0.0;

    double outputWidth = 0.0;
    double outputHeight = 0.0;

    /// Closest the stack may come to an edge of the output it is not anchored
    /// to. A stack flush against the screen edge reads as clipped.
    double screenMargin = 0.0;
};

/// Where the stack goes, in output coordinates.
struct StackPlacement {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const StackPlacement &) const = default;
};

/// Places an open stack against its parent tile.
///
/// Across the dock's axis the stack sits clear of the shelf, on the side away
/// from the screen edge. Along the axis it is centred on the parent tile, then
/// pushed back inside the output if that would hang it off the end — a stack
/// near the corner of the screen has to move rather than be clipped.
StackPlacement placeStack(const StackAnchorParams &p);

}
