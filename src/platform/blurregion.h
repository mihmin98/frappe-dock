#pragma once

#include <QRect>
#include <QRegion>

namespace frappe
{

/// \a rect with its corners rounded to \a radius, as a region cheap enough to
/// send every frame.
///
/// The compositor is told where to blur as a **region**, which is a list of
/// rectangles, and it is told again whenever the shelf moves or grows — which
/// under magnification is every frame. Every rectangle in the region is a
/// separate Wayland request, so the region's rectangle count is not a detail:
/// it is the per-frame cost of the blur.
///
/// The corners are therefore a **bounded staircase**, not a true ellipse.
/// `QRegion(..., QRegion::Ellipse)` builds one rectangle per scanline — 86 of
/// them for the shelf at S = 128 — and sending that four times a frame is what
/// froze the dock during a drag. Here the step count is capped, so the region
/// stays around a dozen rectangles at any tile size.
///
/// The staircase is conservative: every step sits **inside** the true rounded
/// rectangle, so the blur can fall a pixel or two short of the painted corner
/// but never overhangs it. Short is invisible under a translucent shelf;
/// overhanging is a blurred halo outside the dock.
QRegion blurRegion(const QRect &rect, int radius);

}
