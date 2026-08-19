#include "core/geometry/layout.h"
#include "core/geometry/magnification.h"

#include <QRandomGenerator>
#include <QTest>

#include <algorithm>
#include <cmath>

using namespace frappe::geometry;

Q_DECLARE_METATYPE(MagnificationParams)

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

double centreOf(const TilePlacement &t)
{
    return t.offset + t.size / 2.0;
}

double stripLength(const std::vector<TilePlacement> &tiles, double endPad)
{
    return tiles.back().offset + tiles.back().size - tiles.front().offset + 2.0 * endPad;
}

}

/// Magnification. The parameters used here are the prototype's working values,
/// not measurements — the reference screenshot has magnification off and there
/// is nothing in it to read. What is asserted is the *shape* of the behaviour,
/// which is what has to hold whatever 3.13 freezes the numbers at.
class TestMagnification : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void pointerAtCentreMagnifiesCentreTile()
    {
        // Nine tiles, so the shelf has room for the peak. A full shelf is the
        // separate case below.
        const auto base = layout(restingParams(9));
        const std::size_t middle = base.size() / 2;
        const double pointer = centreOf(base[middle]);

        const auto tiles = magnify(base, pointer, tuned(), kAvailable);

        // The tile under the pointer reaches the full magnified size, and is
        // the largest of them all.
        QVERIFY(qAbs(tiles[middle].size - tuned().magnifiedSize) < 0.001);
        for (std::size_t i = 0; i < tiles.size(); ++i) {
            if (i != middle) {
                QVERIFY(tiles[i].size < tiles[middle].size);
            }
        }

        // Sizes fall away monotonically on both sides.
        for (std::size_t i = 1; i <= middle; ++i) {
            QVERIFY(tiles[middle - i].size <= tiles[middle - i + 1].size + 1e-9);
            QVERIFY(tiles[middle + i].size <= tiles[middle + i - 1].size + 1e-9);
        }

        // Tiles outside the falloff radius are untouched.
        QVERIFY(qAbs(tiles.front().size - base.front().size) < 1e-9);
        QVERIFY(qAbs(tiles.back().size - base.back().size) < 1e-9);
    }

    /// Regression: the pointer must stay over the tile it started over.
    ///
    /// The first anchor implementation aligned the strip as a whole, by a single
    /// global ratio. The strip stayed put, but individual tiles slid past the
    /// cursor — so hovering a tile's resting centre magnified, and would have
    /// activated, its *neighbour*. Nothing in the size, continuity or symmetry
    /// assertions could see it; it took hit-testing to expose it. This pins the
    /// property directly: whatever the pointer is over at rest, it is still over
    /// once magnified.
    void pointerStaysOverTheSameTile()
    {
        for (const int count : {5, 21, 40}) {
            const auto base = layout(restingParams(count));

            for (std::size_t i = 0; i < base.size(); ++i) {
                const double pointer = centreOf(base[i]);
                const auto tiles = magnify(base, pointer, tuned(), kAvailable);

                QVERIFY2(pointer >= tiles[i].offset && pointer <= tiles[i].offset + tiles[i].size,
                         qPrintable(QStringLiteral("n=%1: pointer at tile %2's resting centre "
                                                   "left its magnified span")
                                        .arg(count).arg(i)));

                // And it is that tile which grew the most.
                for (std::size_t j = 0; j < tiles.size(); ++j) {
                    if (j != i) {
                        QVERIFY(tiles[j].size <= tiles[i].size + 1e-9);
                    }
                }
            }
        }
    }

    /// When the resting shelf is nearly full, the magnified strip cannot reach
    /// the requested peak without running off the screen. The effect is scaled
    /// back to fit rather than clipped or allowed to overflow: still magnified,
    /// still monotone, just less. This is the common case at the reference size.
    void magnificationClampedWhenShelfIsFull()
    {
        const LayoutParams p = restingParams(21);
        const auto base = layout(p);
        const double endPad = base.front().offset;
        const std::size_t middle = base.size() / 2;

        const auto tiles = magnify(base, centreOf(base[middle]), tuned(), kAvailable);

        // Fills the shelf exactly, and does not exceed it.
        QVERIFY(qAbs(stripLength(tiles, endPad) - kAvailable) < 0.001);

        // Still a magnification: the centre grows, the far ends shrink slightly
        // to pay for it, and the ordering is intact.
        QVERIFY(tiles[middle].size > base[middle].size);
        QVERIFY(tiles[middle].size < tuned().magnifiedSize);
        for (std::size_t i = 1; i <= middle; ++i) {
            QVERIFY(tiles[middle - i].size <= tiles[middle - i + 1].size + 1e-9);
            QVERIFY(tiles[middle + i].size <= tiles[middle + i - 1].size + 1e-9);
        }
    }

    /// Walking the pointer off the end must fade the effect out rather than
    /// dropping it, and once well past the end the layout must be the resting
    /// one again.
    void pointerBeyondEndDegradesSmoothly()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);
        const double shelf = shelfLength(p, base);

        double previous = tuned().magnifiedSize + 1.0;
        for (double pointer = centreOf(base.back()); pointer < shelf + 400.0; pointer += 1.0) {
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);
            QVERIFY2(tiles.back().size <= previous + 1e-9, "size must not grow as the pointer recedes");
            previous = tiles.back().size;
        }

        // Well beyond the falloff radius, nothing is magnified at all.
        const auto far = magnify(base, shelf + 400.0, tuned(), kAvailable);
        for (std::size_t i = 0; i < far.size(); ++i) {
            QVERIFY(qAbs(far[i].size - base[i].size) < 1e-9);
        }
    }

    /// **The critical one.** Discontinuity here is visible as jitter, and is far
    /// easier to assert than to eyeball. Sizes *and* offsets are sampled: a
    /// continuous size curve with a jumping anchor still looks broken.
    void continuity()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);
        const double shelf = shelfLength(p, base);

        constexpr int samples = 20000;
        const double step = (shelf + 200.0) / samples;

        std::vector<TilePlacement> previous = magnify(base, -100.0, tuned(), kAvailable);
        for (int i = 1; i <= samples; ++i) {
            const double pointer = -100.0 + i * step;
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);

            for (std::size_t t = 0; t < tiles.size(); ++t) {
                // A tile may not change size or move faster than a few times
                // the pointer's own speed; anything larger is a jump.
                QVERIFY2(qAbs(tiles[t].size - previous[t].size) < 4.0 * step,
                         qPrintable(QStringLiteral("size jump at pointer %1, tile %2")
                                        .arg(pointer).arg(t)));
                QVERIFY2(qAbs(tiles[t].offset - previous[t].offset) < 8.0 * step,
                         qPrintable(QStringLiteral("offset jump at pointer %1, tile %2")
                                        .arg(pointer).arg(t)));
            }
            previous = tiles;
        }
    }

    /// The curve is symmetric, so a pointer mirrored about the shelf midpoint
    /// must produce the mirrored layout.
    void symmetry()
    {
        const LayoutParams p = restingParams(20);
        const auto base = layout(p);
        const double shelf = shelfLength(p, base);

        for (const double offset : {0.0, 37.0, 120.5, 300.0}) {
            const double left = shelf / 2.0 - offset;
            const double right = shelf / 2.0 + offset;

            const auto a = magnify(base, left, tuned(), kAvailable);
            const auto b = magnify(base, right, tuned(), kAvailable);

            const std::size_t n = a.size();
            for (std::size_t i = 0; i < n; ++i) {
                const TilePlacement &mirrored = b[n - 1 - i];
                QVERIFY2(qAbs(a[i].size - mirrored.size) < 1e-6,
                         qPrintable(QStringLiteral("size asymmetry at offset %1, tile %2")
                                        .arg(offset).arg(i)));
                // Mirror the position about the shelf midpoint too.
                const double reflected = shelf - (mirrored.offset + mirrored.size);
                QVERIFY2(qAbs(a[i].offset - reflected) < 1e-6,
                         qPrintable(QStringLiteral("position asymmetry at offset %1, tile %2")
                                        .arg(offset).arg(i)));
            }
        }
    }

    /// Bit-for-bit, not approximately. This is what keeps the resting dock
    /// matching the reference geometry with the magnification code in the path.
    void disabledMagnificationReturnsBaseLayout_data()
    {
        QTest::addColumn<MagnificationParams>("params");

        QTest::newRow("no magnified size") << MagnificationParams{0.0, 3.0, 1.6};
        QTest::newRow("magnified equals resting") << MagnificationParams{46.5, 3.0, 1.6};
        QTest::newRow("magnified below resting") << MagnificationParams{20.0, 3.0, 1.6};
        QTest::newRow("zero radius") << MagnificationParams{96.0, 0.0, 1.6};
        QTest::newRow("zero exponent") << MagnificationParams{96.0, 3.0, 0.0};
    }

    void disabledMagnificationReturnsBaseLayout()
    {
        QFETCH(MagnificationParams, params);

        const LayoutParams p = restingParams();
        const auto base = layout(p);

        for (const double pointer : {0.0, 300.0, 672.0, 1343.0}) {
            const auto tiles = magnify(base, pointer, params, kAvailable);
            QCOMPARE(tiles.size(), base.size());
            for (std::size_t i = 0; i < base.size(); ++i) {
                QCOMPARE(tiles[i].offset, base[i].offset); // exact, not near
                QCOMPARE(tiles[i].size, base[i].size);
            }
        }
    }

    void emptyLayout()
    {
        QVERIFY(magnify({}, 100.0, tuned(), kAvailable).empty());
    }

    /// Property test: whatever the parameters and wherever the pointer, the
    /// magnified strip fits the shelf.
    void sumOfWidthsNeverExceedsAvailable()
    {
        auto *rng = QRandomGenerator::global();

        for (int i = 0; i < 300; ++i) {
            const int count = rng->bounded(1, 60);
            const double maxSize = 16.0 + rng->bounded(100);
            const MagnificationParams m{
                maxSize * (1.0 + rng->generateDouble() * 3.0),
                0.5 + rng->generateDouble() * 6.0,
                0.4 + rng->generateDouble() * 3.0,
            };

            LayoutParams p{count, maxSize, 24.0, maxSize / 3.0, kAvailable, {}};
            if (count > 3) {
                p.separatorsAfter = {rng->bounded(count - 1)};
            }
            const auto base = layout(p);
            const double endPad = base.front().offset;
            if (shelfLength(p, base) > kAvailable) {
                continue; // already overflowing at rest; that contract is layout's
            }

            for (int s = 0; s <= 40; ++s) {
                const double pointer = s * kAvailable / 40.0;
                const auto tiles = magnify(base, pointer, m, kAvailable);

                const double length = stripLength(tiles, endPad);
                QVERIFY2(length <= kAvailable + 0.001,
                         qPrintable(QStringLiteral("n=%1 max=%2 mag=%3 -> %4")
                                        .arg(count).arg(maxSize).arg(m.magnifiedSize).arg(length)));

                for (const auto &t : tiles) {
                    QVERIFY(t.size >= 0.0);
                    // The peak, except where the resting floor is already above
                    // it — then magnification is off and the floor stands.
                    QVERIFY(t.size <= std::max(m.magnifiedSize, base.front().size) + 0.001);
                    QVERIFY(std::isfinite(t.offset));
                }
            }
        }
    }

    /// Tiles must never overlap or invert order — a magnified strip that folds
    /// over itself would make hit-testing (§3.4) meaningless.
    void tilesStayOrderedAndDisjoint()
    {
        const LayoutParams p = restingParams();
        const auto base = layout(p);

        for (int s = 0; s <= 500; ++s) {
            const double pointer = s * shelfLength(p, base) / 500.0;
            const auto tiles = magnify(base, pointer, tuned(), kAvailable);

            for (std::size_t i = 1; i < tiles.size(); ++i) {
                QVERIFY2(tiles[i].offset >= tiles[i - 1].offset + tiles[i - 1].size - 1e-9,
                         qPrintable(QStringLiteral("overlap at pointer %1, tile %2")
                                        .arg(pointer).arg(i)));
            }
        }
    }
};

QTEST_MAIN(TestMagnification)
#include "test_magnification.moc"
