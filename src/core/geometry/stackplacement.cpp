#include "core/geometry/stackplacement.h"

#include <algorithm>

namespace frappe::geometry
{

namespace
{

/// Keeps \a value between the margins, given a popup of \a extent inside an
/// output of \a available.
///
/// The upper bound is floored at the lower one rather than trusted: a stack
/// wider than the output it is on would otherwise produce an upper bound below
/// the lower one, and std::clamp on an inverted range is undefined. Pinning such
/// a stack to the leading margin is the answer that at least keeps its start
/// visible.
double clampAlongAxis(double value, double extent, double available, double margin)
{
    const double lo = margin;
    const double hi = std::max(lo, available - extent - margin);
    return std::clamp(value, lo, hi);
}

}

StackPlacement placeStack(const StackAnchorParams &p)
{
    // How far the stack's near face sits from the screen edge the dock is on:
    // past the floating gap, past the shelf, plus the clearance.
    const double offEdge = p.shelfGap + p.shelfThickness + p.clearance;

    StackPlacement result;
    switch (p.edge) {
    case DockEdge::Bottom:
        result.x = clampAlongAxis(p.restingTileCentre - p.popupWidth / 2.0, p.popupWidth, p.outputWidth, p.screenMargin);
        result.y = p.outputHeight - offEdge - p.popupHeight;
        return result;

    case DockEdge::Top:
        result.x = clampAlongAxis(p.restingTileCentre - p.popupWidth / 2.0, p.popupWidth, p.outputWidth, p.screenMargin);
        result.y = offEdge;
        return result;

    case DockEdge::Left:
        result.x = offEdge;
        result.y =
            clampAlongAxis(p.restingTileCentre - p.popupHeight / 2.0, p.popupHeight, p.outputHeight, p.screenMargin);
        return result;

    case DockEdge::Right:
        result.x = p.outputWidth - offEdge - p.popupWidth;
        result.y =
            clampAlongAxis(p.restingTileCentre - p.popupHeight / 2.0, p.popupHeight, p.outputHeight, p.screenMargin);
        return result;
    }

    // No default branch: adding an edge must not compile until it is placed.
    return result;
}

}
