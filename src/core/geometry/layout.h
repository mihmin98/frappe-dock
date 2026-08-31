#pragma once

#include <vector>

namespace frappe::geometry
{

/// Everything the resting layout needs. Pure input — no config reads happen
/// below this line.
struct LayoutParams {
    int tileCount = 0;
    double maxTileSize = 0.0; ///< the configured maximum, S in the proportion model
    double minTileSize = 0.0; ///< enforced floor; tiles never compress below it
    double spacing = 0.0;     ///< inter-icon gap at maxTileSize; derived, not configured
    double availableLength = 0.0; ///< usable edge length, screen gap already removed

    /// Tile indices after which a separator sits. A separator is not a tile: it
    /// is a 1 pt rule with 0.44 S of clearance on each side, so it replaces one
    /// ordinary gap with a wider one rather than occupying a cell of its own.
    std::vector<int> separatorsAfter;
};

/// Separator proportions from the reference geometry (plan.md Part 0). The rule
/// is 1 pt and does *not* scale with the tile; only its clearance does.
inline constexpr double separatorClearanceRatio = 0.44;
inline constexpr double separatorLineWidth = 1.0;

struct TilePlacement {
    double offset = 0.0; ///< from the shelf start, i.e. the shelf's outer edge
    double size = 0.0;
};

/// Compression only. No pointer, no magnification.
///
/// Tile size is a maximum: when the content does not fit, every tile shrinks,
/// and the gaps and the shelf padding shrink with it so the whole strip stays
/// proportional. Compression stops at `minTileSize` — past that the strip
/// overflows `availableLength` rather than shrinking further.
std::vector<TilePlacement> layout(const LayoutParams &p);

/// Overall shelf length for a resting layout: the tiles plus one gap of inner
/// padding at each end. Derived from the same placements, never recomputed from
/// the parameters.
double shelfLength(const LayoutParams &p, const std::vector<TilePlacement> &tiles);

/// The shelf's thickness across the dock's axis: the tile plus one gap of
/// padding at each side, S + 2g in the proportion model.
double shelfThickness(double tileSize, double gap);

/// The thickness of the *surface* the shelf lives on, which is not the same
/// number.
///
/// A magnified tile grows away from the screen edge, out of the shelf and past
/// it: at the maximum peak it is several times the shelf's own thickness. A
/// surface sized to the shelf clips it. The surface is therefore sized to hold
/// the largest tile that can ever be drawn on it, keeping the shelf's own
/// geometry — which is normative — unchanged.
///
/// Magnification is not the only thing drawn outside the shelf. Dragging a tile
/// out to remove it carries the artwork a tile's distance clear of the shelf
/// before the gesture even engages, and the Remove label sits beyond that
/// again — so the surface holds `dragOutHeadroom` past the shelf whatever the
/// magnification setting is. Without it the dragged tile and its label are
/// clipped at the shelf edge and the gesture has no visible affordance at all.
///
/// Note what this is *not*: the exclusive zone. The headroom is drawn in but
/// not occupied, and reserving it would push every window up by space the dock
/// only borrows when the pointer is over it. Nor is it the input region: the
/// surface accepts pointer events over the shelf alone.
double surfaceThickness(double tileSize, double gap, double peakTileSize);

/// Room past the shelf for the drag-out gesture, in the proportion model: one
/// tile for the removal threshold, one for the artwork's offset from the
/// pointer, and one for the label above it.
double dragOutHeadroom(double tileSize);

/// Centre position of each separator, in the same coordinates as
/// `TilePlacement::offset` and in the order given by `separatorsAfter`. Derived
/// from the placements so it cannot drift from them.
std::vector<double> separatorCentres(const LayoutParams &p, const std::vector<TilePlacement> &tiles);

}
