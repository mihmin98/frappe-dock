#include "app/dockgeometry.h"

#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>

using namespace frappe;

namespace
{

qreal offsetOf(const QVariantList &list, int row)
{
    return list.at(row).toMap().value(QStringLiteral("offset")).toReal();
}

qreal sizeOf(const QVariantList &list, int row)
{
    return list.at(row).toMap().value(QStringLiteral("size")).toReal();
}

}

/// What this class adds over the pure engine is the translation between the
/// model's rows — separators included — and the engine's tiles, and the promise
/// that the view and the hit test read the same placements. Those are what is
/// tested here; the geometry itself is tested in test_layout and
/// test_magnification.
class TestDockGeometry : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishesOneEntryPerRow()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(5);

        QCOMPARE(g.tileGeometry().size(), 5);
        for (int row = 0; row < 5; ++row) {
            QCOMPARE(sizeOf(g.tileGeometry(), row), 48.0);
        }

        // Pitch is S + S/3 at rest, uniform across the strip.
        QCOMPARE(offsetOf(g.tileGeometry(), 1) - offsetOf(g.tileGeometry(), 0), 64.0);
    }

    void separatorRowsAreGapsNotCells()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(3);
        g.setSeparatorRows({1});

        const QVariantList tiles = g.tileGeometry();
        QCOMPARE(tiles.size(), 3);

        // The two real tiles keep the configured size; the separator row is
        // handed the gap between them instead of a cell of its own.
        QCOMPARE(sizeOf(tiles, 0), 48.0);
        QCOMPARE(sizeOf(tiles, 2), 48.0);

        const qreal gapStart = offsetOf(tiles, 0) + sizeOf(tiles, 0);
        QCOMPARE(offsetOf(tiles, 1), gapStart);
        QCOMPARE(offsetOf(tiles, 2), gapStart + sizeOf(tiles, 1));

        // 0.44 S of clearance each side of a 1 pt rule, wider than the S/3 the
        // ordinary gap would have been.
        QCOMPARE(sizeOf(tiles, 1), 2.0 * 0.44 * 48.0 + 1.0);
    }

    void rowAtAgreesWithThePublishedPlacements()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(4);
        g.setSeparatorRows({2});
        g.setMagnifiedSize(96.0);
        g.setPointerPosition(100.0);

        const QVariantList tiles = g.tileGeometry();
        for (int row = 0; row < tiles.size(); ++row) {
            if (row == 2) {
                continue; // a separator is not a target
            }
            const qreal centre = offsetOf(tiles, row) + sizeOf(tiles, row) / 2.0;
            QCOMPARE(g.rowAt(centre), row);
        }

        // Outside the shelf entirely — which under magnification is not the
        // same as "negative", since the shelf can start before the origin.
        QCOMPARE(g.rowAt(g.shelfStart() - 1.0), -1);
        QCOMPARE(g.rowAt(g.shelfStart() + g.shelfLength() + 1.0), -1);
    }

    void pointerMotionRepublishes()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(6);
        g.setMagnifiedSize(96.0);

        const qreal resting = sizeOf(g.tileGeometry(), 3);

        QSignalSpy spy(&g, &DockGeometry::changed);
        g.setPointerPosition(offsetOf(g.tileGeometry(), 3) + resting / 2.0);

        QCOMPARE(spy.count(), 1);
        QVERIFY(sizeOf(g.tileGeometry(), 3) > resting);

        // Pointer away is the resting layout again, and every "away" position
        // is the same position.
        g.setPointerPosition(-1.0);
        QCOMPARE(sizeOf(g.tileGeometry(), 3), resting);
        const int count = spy.count();
        g.setPointerPosition(-40.0);
        QCOMPARE(spy.count(), count);
    }

    void magnificationOffIgnoresThePointer()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(6);
        g.setMagnifiedSize(96.0);
        g.setMagnificationEnabled(false);

        g.setPointerPosition(100.0);
        for (int row = 0; row < 6; ++row) {
            QCOMPARE(sizeOf(g.tileGeometry(), row), 48.0);
        }
    }

    /// The engine anchors the pointed tile under the cursor, so the magnified
    /// strip can begin before the resting shelf did. What must hold is that the
    /// *shelf* moves and grows to contain it — never that the strip is shifted
    /// back inside a stationary shelf, which would put the pointed tile
    /// somewhere other than under the pointer.
    void theShelfAlwaysContainsTheStrip()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(4);
        g.setSeparatorRows({2});
        g.setMagnifiedSize(96.0);

        const qreal padding = offsetOf(g.tileGeometry(), 0);
        QCOMPARE(g.shelfStart(), 0.0); // at rest, the shelf is where it rests

        for (qreal pointer = 0.0; pointer < 400.0; pointer += 3.0) {
            g.setPointerPosition(pointer);
            const QVariantList tiles = g.tileGeometry();

            // Inside the shelf the layout is unchanged: padding, then the
            // strip, then padding.
            QCOMPARE(offsetOf(tiles, 0) - g.shelfStart(), padding);
            QVERIFY(offsetOf(tiles, 3) + sizeOf(tiles, 3) - g.shelfStart() <= g.shelfLength() + 1e-9);
            QVERIFY(g.shelfLength() >= g.restingLength() - 1e-9);
        }
    }

    /// The whole point of the anchoring, stated in the coordinates the view
    /// draws in: wherever the pointer is, the tile it lands on covers it.
    void thePointedTileStaysUnderThePointer()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(8);
        g.setMagnifiedSize(96.0);

        const QVariantList resting = g.tileGeometry();
        const qreal end = g.restingLength();

        for (qreal pointer = 0.0; pointer <= end; pointer += 1.5) {
            // Which tile the pointer is over *at rest*. A pointer in a gap is
            // over nothing in particular and has nothing to stay over.
            int row = -1;
            for (int i = 0; i < resting.size(); ++i) {
                if (offsetOf(resting, i) <= pointer && pointer <= offsetOf(resting, i) + sizeOf(resting, i)) {
                    row = i;
                    break;
                }
            }
            if (row < 0) {
                continue;
            }

            g.setPointerPosition(pointer);
            const QVariantList tiles = g.tileGeometry();
            QVERIFY2(offsetOf(tiles, row) <= pointer
                         && pointer <= offsetOf(tiles, row) + sizeOf(tiles, row),
                     qPrintable(QStringLiteral("tile %1 stopped covering pointer %2")
                                    .arg(row).arg(pointer)));
            QCOMPARE(g.rowAt(pointer), row);
        }
    }

    void anEmptyDockIsStillAShelf()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);

        QCOMPARE(g.tileGeometry().size(), 0);
        QCOMPARE(g.shelfLength(), 2.0 * 16.0); // padding at each end, nothing between
        QCOMPARE(g.effectiveTileSize(), 48.0);
        QCOMPARE(g.rowAt(0.0), -1);
    }

    /// The harness's one knob. The gap and the shelf's inner padding are the
    /// same constant, so moving the ratio has to move both — a ratio that
    /// changed the gap but not the padding would describe a dock that cannot
    /// exist.
    void spacingRatioMovesGapAndPaddingTogether()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setAvailableLength(1440.0);
        g.setTileCount(3);

        QCOMPARE(g.spacingRatio(), 1.0 / 3.0);
        QCOMPARE(offsetOf(g.tileGeometry(), 0), 16.0);

        g.setSpacingRatio(0.5);

        QCOMPARE(offsetOf(g.tileGeometry(), 0), 24.0); // padding
        QCOMPARE(offsetOf(g.tileGeometry(), 1) - offsetOf(g.tileGeometry(), 0), 72.0); // pitch
    }

    void compressionShrinksTheEffectiveCell()
    {
        DockGeometry g;
        g.setTileSize(48.0);
        g.setMinimumTileSize(8.0);
        g.setAvailableLength(300.0);
        g.setTileCount(20);

        QVERIFY(g.effectiveTileSize() < 48.0);
        QVERIFY(g.shelfLength() <= 300.0 + 0.001);
        QCOMPARE(sizeOf(g.tileGeometry(), 0), g.effectiveTileSize());
    }
};

QTEST_GUILESS_MAIN(TestDockGeometry)
#include "test_dockgeometry.moc"
