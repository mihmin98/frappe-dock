#include "core/geometry/layout.h"

#include <algorithm>

namespace frappe::geometry
{

namespace
{

/// Number of separators that actually sit between two tiles. An index outside
/// [0, tileCount - 2] describes a separator at the very start or end of the
/// strip, which has nothing to separate; it is ignored rather than reserving
/// space for a rule the user cannot see the point of.
int usableSeparators(const LayoutParams &p)
{
    return static_cast<int>(std::ranges::count_if(p.separatorsAfter, [&p](int index) {
        return index >= 0 && index <= p.tileCount - 2;
    }));
}

bool hasSeparatorAfter(const LayoutParams &p, int index)
{
    return index <= p.tileCount - 2 && std::ranges::find(p.separatorsAfter, index) != p.separatorsAfter.end();
}

/// The gap that follows tile `index`, given a compressed tile size.
double gapAfter(const LayoutParams &p, int index, double size, double gapRatio)
{
    if (hasSeparatorAfter(p, index)) {
        return 2.0 * separatorClearanceRatio * size + separatorLineWidth;
    }
    return gapRatio * size;
}

/// The tile size that makes the strip exactly `availableLength` long.
///
/// Solving, with n tiles, k separators and gap ratio r:
///     length = s·n + r·s·(n + 1 - k) + k·(0.88·s + 1)
/// where r·s·(n + 1 - k) covers the ordinary inter-tile gaps plus one gap of
/// shelf padding at each end, and the 1 pt rules are additive because they do
/// not scale.
double fittingSize(const LayoutParams &p, double gapRatio)
{
    // A non-positive length means the caller does not know it yet — the view
    // has not been sized, or there is no output to measure against. Compressing
    // to the floor on no information is worse than laying out at full size and
    // recompressing when the real length arrives.
    if (p.availableLength <= 0.0) {
        return p.maxTileSize;
    }

    const int n = p.tileCount;
    const int k = usableSeparators(p);
    const double denominator =
        n + gapRatio * (n + 1 - k) + 2.0 * separatorClearanceRatio * k;
    if (denominator <= 0.0) {
        return p.maxTileSize;
    }
    return (p.availableLength - k * separatorLineWidth) / denominator;
}

}

std::vector<TilePlacement> layout(const LayoutParams &p)
{
    if (p.tileCount <= 0 || p.maxTileSize <= 0.0) {
        return {};
    }

    // Spacing compresses with the tiles rather than staying fixed, so what the
    // caller passes is read as a ratio of maxTileSize (it is maxTileSize / 3 in
    // production) and reapplied to whatever size we settle on. Holding the gap
    // constant while the tile shrinks would break the proportion model at every
    // size but the configured maximum.
    const double gapRatio = p.spacing / p.maxTileSize;

    double size = std::min(p.maxTileSize, fittingSize(p, gapRatio));
    size = std::max(size, p.minTileSize); // a real floor: overflow, never vanish

    std::vector<TilePlacement> tiles;
    tiles.reserve(static_cast<std::size_t>(p.tileCount));

    double x = gapRatio * size; // shelf inner padding equals the inter-icon gap
    for (int i = 0; i < p.tileCount; ++i) {
        tiles.push_back({x, size});
        x += size + gapAfter(p, i, size, gapRatio);
    }
    return tiles;
}

double shelfLength(const LayoutParams &p, const std::vector<TilePlacement> &tiles)
{
    if (tiles.empty()) {
        return 0.0;
    }
    const double gapRatio = p.maxTileSize > 0.0 ? p.spacing / p.maxTileSize : 0.0;
    const double padding = gapRatio * tiles.front().size;
    return tiles.back().offset + tiles.back().size + padding;
}

double shelfThickness(double tileSize, double gap)
{
    return tileSize + 2.0 * gap;
}

double shelfCornerRadius(double thickness)
{
    // The fitted superellipse in Part 0 comes out at n ~ 2.2 — near enough
    // circular that a plain radius is right and a squircle shader is not.
    return 0.28 * thickness;
}

double surfaceThickness(double tileSize, double gap, double peakTileSize)
{
    // A magnified tile keeps its outer edge on the resting icon's line — one
    // gap in from the shelf's outer edge — and grows inward from there. So the
    // room it needs is that gap plus the tile itself, and never less than the
    // shelf, which is what it is measured against when magnification is off.
    const double peak = std::max(peakTileSize, tileSize);
    const double magnified = std::max(shelfThickness(tileSize, gap), gap + peak);
    return std::max(magnified, shelfThickness(tileSize, gap) + dragOutHeadroom(tileSize));
}

double dragOutHeadroom(double tileSize)
{
    // Dock.qml holds the matching rules: the removal threshold is one tile, the
    // dragged artwork trails the pointer by another, and the label clears that.
    // Three is the sum of those, not a margin chosen for comfort.
    return 3.0 * tileSize;
}

std::vector<double> separatorCentres(const LayoutParams &p, const std::vector<TilePlacement> &tiles)
{
    std::vector<double> centres;
    if (tiles.empty()) {
        return centres;
    }

    centres.reserve(p.separatorsAfter.size());
    for (const int index : p.separatorsAfter) {
        if (index < 0 || index > p.tileCount - 2) {
            continue;
        }
        const TilePlacement &before = tiles[static_cast<std::size_t>(index)];
        const TilePlacement &after = tiles[static_cast<std::size_t>(index) + 1];
        centres.push_back((before.offset + before.size + after.offset) / 2.0);
    }
    return centres;
}

}
