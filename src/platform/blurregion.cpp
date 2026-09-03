#include "platform/blurregion.h"

#include <algorithm>
#include <cmath>

using namespace frappe;

namespace
{
/// Pixels of corner per staircase step, and the hard cap on steps.
///
/// The cap is what bounds the cost; the divisor is what keeps a small corner
/// from being spent on steps it cannot show. At the shelf's radius the two give
/// 2 steps at S = 24 and 8 at S = 128 — 5 and 17 rectangles, against 18 and 86
/// for a scanline ellipse.
constexpr int pixelsPerStep = 6;
constexpr int maximumSteps = 8;

int stepsFor(int radius)
{
    return std::clamp(radius / pixelsPerStep, 2, maximumSteps);
}
}

QRegion frappe::blurRegion(const QRect &rect, int radius)
{
    if (rect.isEmpty()) {
        return {};
    }

    // A radius past half the short side is not a rounder rectangle, it is a
    // different shape; clamping is what the painter does with it too.
    const int r = std::min(radius, std::min(rect.width(), rect.height()) / 2);
    if (r <= 0) {
        return QRegion(rect);
    }

    // The straight middle, full width. Only the caps need shaping.
    QRegion region(rect.adjusted(0, r, 0, -r));

    const int steps = stepsFor(r);
    for (int i = 0; i < steps; ++i) {
        const int near = i * r / steps;
        const int far = (i + 1) * r / steps;

        // Measured at the band's outer edge, where the corner is at its
        // narrowest, so the whole band fits inside the true curve.
        const double fromCentre = r - near;
        const int inset = r - static_cast<int>(std::floor(
                                  std::sqrt(std::max(0.0, double(r) * r - fromCentre * fromCentre))));

        const int width = rect.width() - 2 * inset;
        if (width <= 0) {
            continue;
        }
        const int height = far - near;

        region += QRect(rect.left() + inset, rect.top() + near, width, height);
        region += QRect(rect.left() + inset, rect.bottom() - far + 1, width, height);
    }

    return region;
}
