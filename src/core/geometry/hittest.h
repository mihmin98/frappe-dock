#pragma once

#include "core/geometry/layout.h"
#include "core/geometry/magnification.h"

#include <vector>

namespace frappe::geometry
{

/// Returned when the position falls outside the strip entirely.
inline constexpr int noTile = -1;

/// Which tile is at `pointerPos`, given a layout that is already magnified.
///
/// Hit regions are read off the placements themselves rather than computed
/// alongside them: regions tile the strip contiguously, with the boundary
/// between neighbours at the midpoint of the gap between them. Two independent
/// implementations would drift, and the symptom is clicks landing on the
/// neighbouring tile near the edges — subtly, in a way that feels like a broken
/// mouse rather than a broken dock.
int hitTest(const std::vector<TilePlacement> &tiles, double pointerPos);

/// The same, for callers that have not already magnified. Delegates to the
/// overload above so there is exactly one definition of where a tile is.
int hitTest(const std::vector<TilePlacement> &base,
            double pointerPos,
            const MagnificationParams &m,
            double availableLength);

}
