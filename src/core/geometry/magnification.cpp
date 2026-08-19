#include "core/geometry/magnification.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace frappe::geometry
{

namespace
{

/// Raised cosine, chosen in §3.1 over the truncated Gaussian and the parabolic
/// shape Latte Dock used. All three measured the same for smoothness; the
/// tiebreak is the boundary. The Gaussian has a non-zero slope where it is cut
/// off, so a tile entering the falloff radius starts growing with a visible
/// kick. This reaches zero with zero slope, and has zero slope at the peak too,
/// so there is no cusp under the pointer.
///
/// `u` is distance normalised by the falloff radius; the result is 1 at u = 0,
/// falling monotonically to exactly 0 at u = 1 and staying there.
double falloff(double u, double exponent)
{
    if (u >= 1.0) {
        return 0.0;
    }
    const double raised = 0.5 * (1.0 + std::cos(std::numbers::pi * u));
    return exponent == 1.0 ? raised : std::pow(raised, exponent);
}

double centreOf(const TilePlacement &t)
{
    return t.offset + t.size / 2.0;
}

}

bool magnificationDisabled(const std::vector<TilePlacement> &base, const MagnificationParams &m)
{
    return base.empty() || m.falloffRadius <= 0.0 || m.curveExponent <= 0.0
        || m.magnifiedSize <= base.front().size;
}

std::vector<TilePlacement> magnify(const std::vector<TilePlacement> &base,
                                   double pointerPos,
                                   const MagnificationParams &m,
                                   double availableLength)
{
    // Bit-for-bit, not approximately: this is what makes the resting dock match
    // the reference geometry even with the magnification code in the path.
    if (magnificationDisabled(base, m)) {
        return base;
    }

    const std::size_t count = base.size();
    const double baseSize = base.front().size;

    // Shelf padding equals the inter-icon gap, so the first tile's offset is
    // the gap — that is where the ratio comes from without re-deriving it.
    const double endPad = base.front().offset;
    const double gapRatio = endPad / baseSize;
    const double pitch = baseSize * (1.0 + gapRatio);
    const double radius = m.falloffRadius * pitch;

    std::vector<TilePlacement> tiles(count);

    // Size every tile from its *resting* distance to the pointer. Sizing against
    // the magnified positions instead is a feedback loop, and is where the
    // discontinuities the prototype was built to hunt come from.
    for (std::size_t i = 0; i < count; ++i) {
        const double u = std::abs(centreOf(base[i]) - pointerPos) / radius;
        tiles[i].size = baseSize + (m.magnifiedSize - baseSize) * falloff(u, m.curveExponent);
    }

    // Gaps scale with the tiles they sit between, taken from the resting gap so
    // a separator's wider clearance is carried through without this function
    // needing to know a separator is there.
    const auto gapAfter = [&](std::size_t i) {
        const double restingGap = base[i + 1].offset - (base[i].offset + base[i].size);
        return restingGap * std::max(tiles[i].size, tiles[i + 1].size) / baseSize;
    };

    double interior = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        interior += tiles[i].size;
        if (i + 1 < count) {
            interior += gapAfter(i);
        }
    }

    // The strip may not grow past the shelf. Scaling the interior and leaving
    // the end padding alone keeps "never exceeds available" exact rather than
    // approximate.
    // `gapAfter` is linear in the tile sizes, so scaling the sizes scales the
    // gaps with them and the interior lands exactly on the budget.
    const double budget = availableLength - 2.0 * endPad;
    if (availableLength > 0.0 && interior > budget && interior > 0.0) {
        const double scale = budget / interior;
        for (auto &t : tiles) {
            t.size *= scale;
        }
        interior = budget;
    }

    // Anchor: whatever is under the pointer at rest must still be under it once
    // magnified, and at the same fractional position within its own element.
    //
    // A single global ratio is not good enough here. It keeps the *strip*
    // aligned but lets individual tiles slide past the cursor, so hovering a
    // tile's resting centre can magnify — and activate — its neighbour. That is
    // the defect §3.4 exists to prevent, and it is invisible until you hit-test
    // for it. So the correspondence is built element by element, over the same
    // breakpoints in both layouts: padding, tile, gap, tile, ..., padding.
    const double restingLength = base.back().offset + base.back().size + endPad;
    const double clamped = std::clamp(pointerPos, 0.0, restingLength);

    double restingCursor = 0.0;
    double magnifiedCursor = 0.0;
    double mapped = -1.0;

    const auto consume = [&](double restingSize, double magnifiedSize) {
        if (mapped < 0.0 && clamped <= restingCursor + restingSize) {
            const double t = restingSize > 0.0 ? (clamped - restingCursor) / restingSize : 0.0;
            mapped = magnifiedCursor + t * magnifiedSize;
        }
        restingCursor += restingSize;
        magnifiedCursor += magnifiedSize;
    };

    consume(endPad, endPad);
    for (std::size_t i = 0; i < count; ++i) {
        consume(base[i].size, tiles[i].size);
        if (i + 1 < count) {
            consume(base[i + 1].offset - (base[i].offset + base[i].size), gapAfter(i));
        }
    }
    consume(endPad, endPad);
    if (mapped < 0.0) {
        mapped = magnifiedCursor;
    }

    double x = clamped - mapped + endPad;
    for (std::size_t i = 0; i < count; ++i) {
        const double size = tiles[i].size;
        tiles[i].offset = x;
        x += size;
        if (i + 1 < count) {
            x += gapAfter(i);
        }
    }
    return tiles;
}

}
