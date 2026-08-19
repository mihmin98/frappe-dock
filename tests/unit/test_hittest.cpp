#include "core/geometry/hittest.h"
#include "core/geometry/layout.h"
#include "core/geometry/magnification.h"

#include <QTest>

using namespace frappe::geometry;

namespace
{

constexpr double kAvailable = 1440.0;

LayoutParams restingParams(int count = 21, double maxSize = 46.5)
{
    return LayoutParams{count, maxSize, 24.0, maxSize / 3.0, kAvailable, {}};
}

MagnificationParams tuned()
{
    return MagnificationParams{96.0, 3.0, 1.6};
}

}

/// Hit-testing under transform. Magnified tiles move, and if hit regions are
/// computed independently of the layout they drift apart — the symptom being
/// clicks landing on the neighbouring tile near the edges, which feels like a
/// broken mouse rather than a broken dock.
class TestHitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    /// The core claim: wherever the pointer is, the tile hit-testing names is
    /// the tile whose magnified span actually contains the pointer. Must hold
    /// everywhere, not just at tile centres.
    void hitTestAgreesWithLayout()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);
        const double shelf = shelfLength(p, base);

        constexpr int samples = 10000;
        int insideTiles = 0;

        for (int i = 0; i <= samples; ++i) {
            const double pointer = i * shelf / samples;
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);
            const int hit = hitTest(tiles, pointer);

            if (hit == noTile) {
                // Only the shelf's end padding may report no tile.
                QVERIFY2(pointer < tiles.front().offset
                             || pointer >= tiles.back().offset + tiles.back().size,
                         qPrintable(QStringLiteral("no tile inside the strip at %1").arg(pointer)));
                continue;
            }

            const TilePlacement &t = tiles[std::size_t(hit)];
            if (pointer >= t.offset && pointer < t.offset + t.size) {
                ++insideTiles;
                continue;
            }

            // Otherwise the pointer is in a gap, and must be nearer to the
            // named tile than to any other.
            const double distance = pointer < t.offset ? t.offset - pointer
                                                       : pointer - (t.offset + t.size);
            for (std::size_t j = 0; j < tiles.size(); ++j) {
                if (int(j) == hit) {
                    continue;
                }
                const TilePlacement &o = tiles[j];
                const double other = pointer < o.offset ? o.offset - pointer
                                                        : pointer - (o.offset + o.size);
                QVERIFY2(distance <= other + 1e-9,
                         qPrintable(QStringLiteral("hit %1 at %2 but %3 is nearer")
                                        .arg(hit).arg(pointer).arg(j)));
            }
        }

        // The gap-only path would satisfy the above vacuously; most samples
        // must actually land on a tile.
        QVERIFY(insideTiles > samples / 2);
    }

    /// Under magnification the tile under the pointer is the one the pointer is
    /// over — not its neighbour. This is the documented defect the derivation
    /// rule exists to prevent, checked at the point it would appear: the edges
    /// of a tile that has grown.
    void magnifiedTileEdgesHitThemselves()
    {
        const auto base = layout(restingParams());

        for (std::size_t i = 0; i < base.size(); ++i) {
            const double pointer = base[i].offset + base[i].size / 2.0;
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);

            QCOMPARE(hitTest(tiles, pointer), int(i));

            // Just inside each edge of the tile as it is actually drawn.
            const TilePlacement &t = tiles[i];
            QCOMPARE(hitTest(tiles, t.offset + 0.01), int(i));
            QCOMPARE(hitTest(tiles, t.offset + t.size - 0.01), int(i));
        }
    }

    /// Exactly one tile at every position: regions are contiguous inside the
    /// strip, never overlapping and never leaving a hole.
    void boundariesAreUnambiguous()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);
        const double shelf = shelfLength(p, base);

        constexpr int samples = 20000;
        int previous = noTile;

        for (int i = 0; i <= samples; ++i) {
            const double pointer = i * shelf / samples;
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);
            const int hit = hitTest(tiles, pointer);

            // Inside the strip there is always exactly one answer, and it is
            // consistent with the spans: no position resolves to two tiles.
            if (pointer >= tiles.front().offset
                && pointer < tiles.back().offset + tiles.back().size) {
                QVERIFY2(hit != noTile,
                         qPrintable(QStringLiteral("hole at %1").arg(pointer)));

                int containing = 0;
                for (const auto &t : tiles) {
                    if (pointer >= t.offset && pointer < t.offset + t.size) {
                        ++containing;
                    }
                }
                QVERIFY2(containing <= 1,
                         qPrintable(QStringLiteral("%1 tiles contain %2").arg(containing).arg(pointer)));
            }

            // The index may only ever step to a neighbour as the pointer
            // advances — a jump means the regions are not contiguous.
            if (previous != noTile && hit != noTile) {
                QVERIFY2(qAbs(hit - previous) <= 1,
                         qPrintable(QStringLiteral("index jumped %1 -> %2 at %3")
                                        .arg(previous).arg(hit).arg(pointer)));
            }
            previous = hit;
        }
    }

    void outsideTheStripIsNoTile()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);
        const auto tiles = magnify(base, 0.0, tuned(), kAvailable);

        QCOMPARE(hitTest(tiles, tiles.front().offset - 0.01), noTile);
        QCOMPARE(hitTest(tiles, tiles.back().offset + tiles.back().size), noTile);
        QCOMPARE(hitTest(tiles, -500.0), noTile);
        QCOMPARE(hitTest(tiles, 99999.0), noTile);
        QCOMPARE(hitTest({}, 100.0), noTile);
    }

    /// The convenience overload must not become a second implementation.
    void overloadsAgree()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);

        for (int i = 0; i <= 2000; ++i) {
            const double pointer = i * shelfLength(p, base) / 2000.0;
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);
            QCOMPARE(hitTest(base, pointer, tuned(), kAvailable), hitTest(tiles, pointer));
        }
    }

    /// 7.7 lets users change the curve settings, so agreement has to hold
    /// across the whole permitted parameter space, not just at the defaults.
    void agreementHoldsAcrossParameterSpace_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<double>("tileSize");
        QTest::addColumn<double>("magnified");
        QTest::addColumn<double>("radius");
        QTest::addColumn<double>("exponent");

        QTest::newRow("defaults") << 21 << 46.5 << 96.0 << 3.0 << 1.6;
        QTest::newRow("magnification off") << 21 << 46.5 << 0.0 << 3.0 << 1.6;
        QTest::newRow("tiny dock") << 1 << 24.0 << 64.0 << 3.0 << 1.6;
        QTest::newRow("two tiles") << 2 << 48.0 << 128.0 << 2.0 << 1.0;
        QTest::newRow("crowded") << 40 << 46.5 << 96.0 << 3.0 << 1.6;
        QTest::newRow("very crowded") << 80 << 32.0 << 96.0 << 4.0 << 2.0;
        QTest::newRow("narrow falloff") << 21 << 46.5 << 96.0 << 0.6 << 1.0;
        QTest::newRow("wide falloff") << 21 << 46.5 << 96.0 << 8.0 << 1.0;
        QTest::newRow("flat curve") << 21 << 46.5 << 96.0 << 3.0 << 0.3;
        QTest::newRow("steep curve") << 21 << 46.5 << 96.0 << 3.0 << 6.0;
        QTest::newRow("huge magnification") << 12 << 32.0 << 160.0 << 3.0 << 1.6;
        QTest::newRow("large tiles") << 8 << 128.0 << 192.0 << 2.5 << 1.6;
    }

    void agreementHoldsAcrossParameterSpace()
    {
        QFETCH(int, count);
        QFETCH(double, tileSize);
        QFETCH(double, magnified);
        QFETCH(double, radius);
        QFETCH(double, exponent);

        const LayoutParams p = restingParams(count, tileSize);
        const MagnificationParams m{magnified, radius, exponent};
        const auto base = layout(p);
        const double shelf = shelfLength(p, base);

        int previous = noTile;
        for (int i = 0; i <= 4000; ++i) {
            const double pointer = i * shelf / 4000.0;
            const auto tiles = magnify(base, pointer, m, kAvailable);
            const int hit = hitTest(tiles, pointer);

            if (hit != noTile) {
                const TilePlacement &t = tiles[std::size_t(hit)];
                const double slack = (t.size + tileSize) / 2.0;
                QVERIFY2(pointer > t.offset - slack && pointer < t.offset + t.size + slack,
                         qPrintable(QStringLiteral("hit %1 is not near %2").arg(hit).arg(pointer)));
            }
            if (previous != noTile && hit != noTile) {
                QVERIFY2(qAbs(hit - previous) <= 1,
                         qPrintable(QStringLiteral("index jumped %1 -> %2 at %3")
                                        .arg(previous).arg(hit).arg(pointer)));
            }
            previous = hit;
        }
    }
};

QTEST_MAIN(TestHitTest)
#include "test_hittest.moc"
