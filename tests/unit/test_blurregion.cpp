#include "platform/blurregion.h"

#include "core/geometry/layout.h"

#include <QTest>

using namespace frappe;

namespace
{
/// The shelf's own geometry at a given tile size, which is what this region is
/// always built from in practice.
QRect shelfAt(double tileSize, int length = 900)
{
    const double thickness = geometry::shelfThickness(tileSize, tileSize / 3.0);
    return QRect(200, 800, length, qRound(thickness));
}

int radiusAt(double tileSize)
{
    return qRound(geometry::shelfCornerRadius(geometry::shelfThickness(tileSize, tileSize / 3.0)));
}
}

/*
 * The blur region.
 *
 * Regression, found by manual testing on 2026-09-03: dragging a tile out of the
 * dock and moving it around froze the dock. The region is re-sent to the
 * compositor whenever the shelf moves or grows, which under magnification is
 * every frame, and each *rectangle* in it is a separate Wayland request. The
 * corners were built with QRegion::Ellipse, which is one rectangle per scanline
 * — 86 of them for the shelf at S = 128 — and the shelf's x, y, width and height
 * each report separately, so the dock was issuing roughly 350 region requests
 * per frame. rectangleCountStaysBounded() is the test that fails if that comes
 * back; the coalescing that turns four calls per frame into one is in the
 * surface factory.
 */
class TestBlurRegion : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /// The regression. A region whose cost scales with the radius is a region
    /// that gets more expensive exactly as the dock gets bigger.
    void rectangleCountStaysBounded()
    {
        for (const double tileSize : {24.0, 48.0, 64.0, 96.0, 128.0}) {
            const QRegion region = blurRegion(shelfAt(tileSize), radiusAt(tileSize));
            QVERIFY2(region.rectCount() <= 20,
                     qPrintable(QStringLiteral("S = %1 produced %2 rectangles")
                                    .arg(tileSize)
                                    .arg(region.rectCount())));
        }
    }

    /// It is still a rounded rectangle: the straight edges are covered and the
    /// extreme corners are not.
    void roundsTheCornersItIsGiven()
    {
        const QRect shelf = shelfAt(64.0);
        const int radius = radiusAt(64.0);
        const QRegion region = blurRegion(shelf, radius);

        // Mid-edge, all four sides.
        QVERIFY(region.contains(QPoint(shelf.center().x(), shelf.top())));
        QVERIFY(region.contains(QPoint(shelf.center().x(), shelf.bottom())));
        QVERIFY(region.contains(QPoint(shelf.left(), shelf.center().y())));
        QVERIFY(region.contains(QPoint(shelf.right(), shelf.center().y())));

        // The corners themselves are outside a rounded rectangle.
        QVERIFY(!region.contains(shelf.topLeft()));
        QVERIFY(!region.contains(shelf.topRight()));
        QVERIFY(!region.contains(shelf.bottomLeft()));
        QVERIFY(!region.contains(shelf.bottomRight()));
    }

    /// The staircase falls inside the true curve, never outside it.
    ///
    /// Which way it errs matters: short of the painted corner is invisible under
    /// a translucent shelf, and over it is a blurred halo hanging off the dock.
    void staysInsideTheTrueCurve()
    {
        const QRect shelf = shelfAt(128.0);
        const int radius = radiusAt(128.0);
        const QRegion region = blurRegion(shelf, radius);

        const QPointF centre(shelf.left() + radius, shelf.top() + radius);
        for (int y = shelf.top(); y < shelf.top() + radius; ++y) {
            for (int x = shelf.left(); x < shelf.left() + radius; ++x) {
                if (!region.contains(QPoint(x, y))) {
                    continue;
                }
                // Half a pixel of slack: the region is whole pixels and the
                // curve is not.
                const double dx = centre.x() - x;
                const double dy = centre.y() - y;
                QVERIFY2(std::sqrt(dx * dx + dy * dy) <= radius + 0.5,
                         qPrintable(QStringLiteral("(%1,%2) is outside the corner").arg(x).arg(y)));
            }
        }
    }

    /// No radius is a plain rectangle, in one rectangle.
    void zeroRadiusIsThePlainRectangle()
    {
        const QRect shelf = shelfAt(64.0);
        const QRegion region = blurRegion(shelf, 0);

        QCOMPARE(region.rectCount(), 1);
        QCOMPARE(region.boundingRect(), shelf);
    }

    /// A radius past half the short side is a different shape, not a rounder
    /// one. Clamping is what the painter does with it too.
    void oversizedRadiusIsClamped()
    {
        const QRect shelf = shelfAt(64.0);
        const QRegion region = blurRegion(shelf, 10 * shelf.height());

        QVERIFY(region.rectCount() <= 20);
        QCOMPARE(region.boundingRect().height(), shelf.height());
        QVERIFY(region.contains(shelf.center()));
    }

    /// An empty shelf asks for nothing rather than for a degenerate rectangle.
    void emptyRectangleIsEmptyRegion()
    {
        QVERIFY(blurRegion(QRect(), 10).isEmpty());
        QVERIFY(blurRegion(QRect(10, 10, 0, 50), 10).isEmpty());
    }
};

QTEST_MAIN(TestBlurRegion)
#include "test_blurregion.moc"
