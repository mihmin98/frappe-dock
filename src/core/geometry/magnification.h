#pragma once

#include "core/geometry/layout.h"

#include <vector>

namespace frappe::geometry
{

/// Nothing here can be read off the reference screenshot — it shows the dock at
/// rest, with magnification off. These come from the tuning harness (§3.5) and
/// are frozen as config defaults in §3.13.
struct MagnificationParams {
    double magnifiedSize = 0.0;  ///< size of the tile directly under the pointer
    double falloffRadius = 0.0;  ///< reach of the effect, in cell pitches
    double curveExponent = 1.0;  ///< shape of the falloff; 1.0 is the plain raised cosine
};

/// True when the parameters describe no magnification at all, in which case
/// `magnify()` returns the resting layout unchanged.
bool magnificationDisabled(const std::vector<TilePlacement> &base, const MagnificationParams &m);

/// Takes the resting layout and the pointer position; returns the magnified
/// one. Pure, deterministic, no state.
///
/// `pointerPos` is in the same coordinates as `TilePlacement::offset` — measured
/// from the shelf start, not from the screen edge.
///
/// This runs on every pointer motion event, so it allocates nothing beyond the
/// result vector, reads no config, and computes nothing lazily.
std::vector<TilePlacement> magnify(const std::vector<TilePlacement> &base,
                                   double pointerPos,
                                   const MagnificationParams &m,
                                   double availableLength);

}
