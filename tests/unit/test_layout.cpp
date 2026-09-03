#include "core/geometry/layout.h"

#include <QRandomGenerator>
#include <QTest>

using namespace frappe::geometry;

namespace
{

/// Production always derives spacing from the tile size; tests that are not
/// specifically sweeping the ratio should do the same.
LayoutParams params(int count, double maxSize, double minSize, double available)
{
    return LayoutParams{count, maxSize, minSize, maxSize / 3.0, available, {}};
}

}

/// The compression half of the geometry engine: tile size is a maximum, not a
/// fixed value, and everything derives from it.
class TestLayout : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // -- Thickness ----------------------------------------------------------

    /// Regression, reported during the §3.5.5 tuning pass: at the maximum peak
    /// size the magnified icons were cut off. The surface was sized to the
    /// shelf, and a tile magnified past the shelf's own thickness had nowhere
    /// to be drawn.
    void theSurfaceHoldsTheLargestTileItCanDraw()
    {
        const double S = 48.0;
        const double gap = S / 3.0;

        // The shelf itself is unchanged by any of this: it is normative
        // geometry and does not grow with the magnification setting.
        QCOMPARE(shelfThickness(S, gap), 80.0);

        // Regression: the surface used to equal the shelf whenever the peak was
        // no taller than the tile, so with magnification off there was no room
        // outside the shelf at all — a tile dragged out to be removed vanished
        // at the shelf edge and the Remove label was never seen. The drag-out
        // gesture needs its headroom whatever magnification is doing.
        QCOMPARE(surfaceThickness(S, gap, S), shelfThickness(S, gap) + dragOutHeadroom(S));
        QCOMPARE(surfaceThickness(S, gap, 0.0), shelfThickness(S, gap) + dragOutHeadroom(S));

        // Magnification is accounted for by the same maximum, so a peak that
        // needs more room than the drag does still gets it.
        QCOMPARE(surfaceThickness(S, gap, 12.0 * S), gap + 12.0 * S);

        // And it never shrinks below the shelf as the peak rises from nothing.
        for (double factor = 0.5; factor <= 3.0; factor += 0.05) {
            QVERIFY(surfaceThickness(S, gap, factor * S) >= shelfThickness(S, gap));
        }
    }

    /// The headroom is in the proportion model too, so the affordance is the
    /// same shape at every tile size rather than cramped at the small end.
    void dragOutHeadroomScalesWithTheTile()
    {
        for (const double S : {24.0, 46.5, 48.0, 128.0}) {
            QCOMPARE(dragOutHeadroom(S) / S, 3.0);
        }
    }

    /// Thickness is in the proportion model like everything else: a dock at
    /// S = 24 is a uniform scale of one at S = 128.
    void thicknessScalesWithTheTile()
    {
        for (const double S : {24.0, 46.5, 48.0, 128.0}) {
            const double gap = S / 3.0;
            QCOMPARE(shelfThickness(S, gap) / S, 5.0 / 3.0);
            QCOMPARE(surfaceThickness(S, gap, 2.0 * S) / S, 5.0 / 3.0 + 3.0);
            // The corner is 0.28 of the thickness, which is 0.47 S. The blur
            // region is rounded to this same number, so a radius that stopped
            // scaling would show up as a blur that no longer fits its shelf.
            QCOMPARE(shelfCornerRadius(shelfThickness(S, gap)) / shelfThickness(S, gap), 0.28);
        }
    }

    // -- The tie to the reference geometry ---------------------------------

    /// The one test that ties the engine to the reference screenshot: 21 tiles
    /// and one separator on a 1440 pt edge at S = 46.5. If the proportion model
    /// in plan.md Part 0 is ever quietly changed, this fails.
    ///
    /// Tolerances are +-1-2 pt because the screenshot cannot resolve where the
    /// tile's true bounds sit relative to the visible artwork. Do not tighten
    /// them; that is fitting to antialiasing.
    void referenceDockReproducesScreenshot()
    {
        LayoutParams p = params(21, 46.5, 24.0, 1440.0);
        p.separatorsAfter = {18}; // 19 apps, a separator, a stack, Trash

        const auto tiles = layout(p);
        QCOMPARE(tiles.size(), std::size_t(21));

        // No compression at this size: the dock fits with room to spare.
        QVERIFY(qAbs(tiles.front().size - 46.5) < 0.001);

        const double pitch = tiles[1].offset - tiles[0].offset;
        QVERIFY2(qAbs(pitch - 62.0) <= 1.0, qPrintable(QString::number(pitch)));

        QVERIFY2(qAbs(tiles.front().offset - 15.5) <= 1.0,
                 qPrintable(QString::number(tiles.front().offset)));

        const double length = shelfLength(p, tiles);
        QVERIFY2(qAbs(length - 1344.0) <= 2.0, qPrintable(QString::number(length)));

        // The shelf is centred, so its midpoint must land on the screen's.
        const double shelfStart = (1440.0 - length) / 2.0;
        QVERIFY(qAbs(shelfStart + length / 2.0 - 720.0) <= 1.0);
    }

    /// The separator is a rule with clearance, not a cell: 0.88 S + 1 art edge
    /// to art edge against 42 pt measured. Treating it as an ordinary tile
    /// would make the shelf roughly half a tile-width too long.
    void separatorIsNotATile()
    {
        LayoutParams p = params(21, 46.5, 24.0, 1440.0);
        p.separatorsAfter = {18};
        const auto tiles = layout(p);

        const double separatorGap = tiles[19].offset - (tiles[18].offset + tiles[18].size);
        QVERIFY2(qAbs(separatorGap - 42.0) <= 1.0, qPrintable(QString::number(separatorGap)));

        // Every other gap is the ordinary one.
        const double ordinary = tiles[1].offset - (tiles[0].offset + tiles[0].size);
        QVERIFY(qAbs(ordinary - 46.5 / 3.0) < 0.001);

        const auto centres = separatorCentres(p, tiles);
        QCOMPARE(centres.size(), std::size_t(1));
        QVERIFY(qAbs(centres.front() - (tiles[18].offset + tiles[18].size + separatorGap / 2.0)) < 0.001);
    }

    /// A separator with nothing after it has nothing to separate, so it must
    /// not reserve space and lengthen the shelf.
    void separatorPastTheEndIsIgnored()
    {
        LayoutParams p = params(5, 48.0, 24.0, 1440.0);
        const auto plain = layout(p);

        p.separatorsAfter = {4, 17, -1};
        const auto withTrailing = layout(p);

        QCOMPARE(withTrailing.size(), plain.size());
        for (std::size_t i = 0; i < plain.size(); ++i) {
            QVERIFY(qAbs(withTrailing[i].offset - plain[i].offset) < 0.001);
            QVERIFY(qAbs(withTrailing[i].size - plain[i].size) < 0.001);
        }
        QVERIFY(separatorCentres(p, withTrailing).empty());
    }

    // -- Compression -------------------------------------------------------

    void compression_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<double>("maxSize");
        QTest::addColumn<double>("minSize");
        QTest::addColumn<double>("available");
        QTest::addColumn<bool>("compressed");

        QTest::newRow("threeTilesInWideDock") << 3 << 48.0 << 24.0 << 1440.0 << false;
        QTest::newRow("fortyTiles") << 40 << 48.0 << 24.0 << 1440.0 << true;
        QTest::newRow("oneTile") << 1 << 48.0 << 24.0 << 1440.0 << false;
        QTest::newRow("hundredTiles") << 100 << 48.0 << 24.0 << 1440.0 << true;
    }

    void compression()
    {
        QFETCH(int, count);
        QFETCH(double, maxSize);
        QFETCH(double, minSize);
        QFETCH(double, available);
        QFETCH(bool, compressed);

        const LayoutParams p = params(count, maxSize, minSize, available);
        const auto tiles = layout(p);

        QCOMPARE(tiles.size(), std::size_t(count));
        for (const auto &t : tiles) {
            QVERIFY(t.size >= minSize);              // the floor holds
            QVERIFY(t.size <= maxSize + 0.001);      // never larger than configured
            QVERIFY(qAbs(t.size - tiles.front().size) < 0.001); // uniform
        }

        if (compressed) {
            QVERIFY(tiles.front().size < maxSize);
        } else {
            QVERIFY(qAbs(tiles.front().size - maxSize) < 0.001);
            QVERIFY(shelfLength(p, tiles) <= available + 0.001);
        }
    }

    /// The boundary: content that fits exactly must not compress at all, and
    /// one pt less must compress at least a little.
    void exactlyFitting()
    {
        const int count = 12;
        const double maxSize = 48.0;
        const double gap = maxSize / 3.0;
        const double exact = count * maxSize + (count + 1) * gap;

        auto tiles = layout(params(count, maxSize, 24.0, exact));
        QVERIFY(qAbs(tiles.front().size - maxSize) < 0.001);
        QVERIFY(qAbs(shelfLength(params(count, maxSize, 24.0, exact), tiles) - exact) < 0.001);

        tiles = layout(params(count, maxSize, 24.0, exact - 1.0));
        QVERIFY(tiles.front().size < maxSize);
    }

    void zeroTiles()
    {
        const LayoutParams p = params(0, 48.0, 24.0, 1440.0);
        QVERIFY(layout(p).empty());
        QCOMPARE(shelfLength(p, {}), 0.0);
        QVERIFY(separatorCentres(p, {}).empty());
    }

    /// Documented behaviour when even the floor does not fit: tiles stay at the
    /// floor and the strip overflows. Chosen over clipping because an overflow
    /// is visible and self-diagnosing, where a clip silently hides tiles the
    /// user pinned.
    void narrowerThanOneTileAtFloor()
    {
        const LayoutParams p = params(3, 48.0, 24.0, 20.0);
        const auto tiles = layout(p);

        QCOMPARE(tiles.size(), std::size_t(3));
        for (const auto &t : tiles) {
            QCOMPARE(t.size, 24.0);
        }
        QVERIFY(shelfLength(p, tiles) > 20.0); // overflow, by design
    }

    // -- Properties --------------------------------------------------------

    /// Whenever the floor fits, the strip fits. (When it does not, the
    /// documented behaviour is the overflow above, so those parameter sets are
    /// excluded rather than silently passed.)
    void totalWidthNeverExceedsAvailable()
    {
        auto *rng = QRandomGenerator::global();
        int checked = 0;

        for (int i = 0; i < 500; ++i) {
            const int count = rng->bounded(1, 80);
            const double maxSize = 16.0 + rng->bounded(120);
            const double minSize = 8.0 + rng->bounded(qMax(1, int(maxSize) - 8));
            const double available = 200.0 + rng->bounded(3000);
            // Sweep the spacing ratio too — it is a parameter, not a setting,
            // precisely so this property can be checked away from 1/3.
            const double spacing = maxSize * (0.1 + rng->generateDouble() * 0.6);

            LayoutParams p{count, maxSize, minSize, spacing, available, {}};
            if (count > 3) {
                p.separatorsAfter = {rng->bounded(count - 1)};
            }

            const auto tiles = layout(p);
            QCOMPARE(tiles.size(), std::size_t(count));

            // Would the floor itself fit? If not, overflow is the contract.
            const LayoutParams atFloor{count, minSize, minSize, spacing * minSize / maxSize,
                                       available, p.separatorsAfter};
            if (shelfLength(atFloor, layout(atFloor)) > available) {
                continue;
            }

            const double length = shelfLength(p, tiles);
            QVERIFY2(length <= available + 0.001,
                     qPrintable(QStringLiteral("n=%1 max=%2 min=%3 avail=%4 -> %5")
                                    .arg(count).arg(maxSize).arg(minSize).arg(available).arg(length)));

            for (const auto &t : tiles) {
                QVERIFY(t.size >= minSize - 0.001);
                QVERIFY(t.size <= maxSize + 0.001);
            }
            ++checked;
        }

        // Guard against the property quietly becoming vacuous if the skip
        // condition above ever widens.
        QVERIFY2(checked > 100, qPrintable(QStringLiteral("only %1 cases checked").arg(checked)));
    }

    /// Gaps and shelf padding are one constant, and both compress with the
    /// tiles rather than staying fixed at the configured value.
    void spacingRespected()
    {
        for (const int count : {2, 5, 40}) {
            const LayoutParams p = params(count, 48.0, 8.0, 900.0);
            const auto tiles = layout(p);
            const double expectedGap = tiles.front().size / 3.0;

            QVERIFY2(qAbs(tiles.front().offset - expectedGap) < 0.001,
                     "shelf padding must equal the inter-icon gap");

            for (std::size_t i = 1; i < tiles.size(); ++i) {
                const double gap = tiles[i].offset - (tiles[i - 1].offset + tiles[i - 1].size);
                QVERIFY2(qAbs(gap - expectedGap) < 0.001, qPrintable(QString::number(gap)));
            }
        }
    }

    /// A dock at S = 24 must be a uniform scale of a dock at S = 128: no
    /// dimension in the engine is a literal.
    void uniformScaleAcrossTileSizes()
    {
        const auto small = layout(params(10, 24.0, 8.0, 4000.0));
        const auto large = layout(params(10, 128.0, 8.0, 4000.0));

        const double factor = 128.0 / 24.0;
        for (std::size_t i = 0; i < small.size(); ++i) {
            QVERIFY(qAbs(large[i].offset - small[i].offset * factor) < 0.001);
            QVERIFY(qAbs(large[i].size - small[i].size * factor) < 0.001);
        }
    }
};

QTEST_MAIN(TestLayout)
#include "test_layout.moc"
