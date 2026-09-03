#include "app/dockgeometry.h"
#include "core/geometry/stackplacement.h"

#include <QTest>
#include <QVariantMap>

#include <cmath>

using namespace frappe;
using namespace frappe::geometry;

namespace
{

/// A dock at S = 48 on a 1920x1080 output, which is the shape every assertion
/// below is about unless it says otherwise.
StackAnchorParams params(DockEdge edge)
{
    StackAnchorParams p;
    p.edge = edge;
    // Along the dock's axis, which is the vertical one for a side dock. Using
    // the horizontal midpoint for those would put the anchor past the bottom of
    // a 1080-tall output and the assertions would be about the clamp instead.
    p.restingTileCentre = (edge == DockEdge::Left || edge == DockEdge::Right) ? 540.0 : 960.0;
    p.shelfGap = 16.0 / 3.0; // g/3
    p.shelfThickness = 48.0 + 2.0 * 16.0; // S + 2g
    p.clearance = 16.0; // g
    p.popupWidth = 400.0;
    p.popupHeight = 300.0;
    p.outputWidth = 1920.0;
    p.outputHeight = 1080.0;
    p.screenMargin = 16.0;
    return p;
}

/// How far the stack's near face should sit from the anchored edge.
double offEdge(const StackAnchorParams &p)
{
    return p.shelfGap + p.shelfThickness + p.clearance;
}

}

/// Where an open stack goes, and — the point of the whole exercise — that
/// pointer movement cannot move it.
class TestStackPositioning : public QObject
{
    Q_OBJECT

private:
    /// A dock geometry with \a count tiles, laid out and ready to query.
    static void configure(DockGeometry &geometry, int count)
    {
        geometry.setTileSize(48.0);
        geometry.setMinimumTileSize(24.0);
        geometry.setAvailableLength(1920.0);
        geometry.setSpacingRatio(1.0 / 3.0);
        geometry.setMagnificationEnabled(true);
        geometry.setMagnifiedSize(96.0);
        geometry.setFalloffRadius(3.0);
        geometry.setCurveExponent(1.6);
        geometry.setTileCount(count);
    }

private Q_SLOTS:
    void positionCorrectForBottomDock()
    {
        const StackAnchorParams p = params(DockEdge::Bottom);
        const StackPlacement placed = placeStack(p);

        // Above the shelf, clear of it, and centred on the tile.
        QCOMPARE(placed.y, p.outputHeight - offEdge(p) - p.popupHeight);
        QCOMPARE(placed.x, p.restingTileCentre - p.popupWidth / 2.0);
        QVERIFY(placed.y + p.popupHeight < p.outputHeight - p.shelfGap - p.shelfThickness);
    }

    void positionCorrectForTopDock()
    {
        const StackAnchorParams p = params(DockEdge::Top);
        const StackPlacement placed = placeStack(p);

        // The mirror of Bottom: below the shelf rather than above it.
        QCOMPARE(placed.y, offEdge(p));
        QCOMPARE(placed.x, p.restingTileCentre - p.popupWidth / 2.0);
    }

    void positionCorrectForLeftDock()
    {
        const StackAnchorParams p = params(DockEdge::Left);
        const StackPlacement placed = placeStack(p);

        // To the right of the shelf, centred on the tile along the vertical.
        QCOMPARE(placed.x, offEdge(p));
        QCOMPARE(placed.y, p.restingTileCentre - p.popupHeight / 2.0);
    }

    void positionCorrectForRightDock()
    {
        const StackAnchorParams p = params(DockEdge::Right);
        const StackPlacement placed = placeStack(p);

        // To the left of the shelf, so the stack never covers the dock.
        QCOMPARE(placed.x, p.outputWidth - offEdge(p) - p.popupWidth);
        QVERIFY(placed.x + p.popupWidth < p.outputWidth - p.shelfGap - p.shelfThickness);
        QCOMPARE(placed.y, p.restingTileCentre - p.popupHeight / 2.0);
    }

    void stackNeverOverlapsTheShelf()
    {
        // Every edge, checked the same way: the stack's near face is past the
        // shelf's inner face by the clearance, whichever axis that is on.
        for (DockEdge edge : {DockEdge::Bottom, DockEdge::Top, DockEdge::Left, DockEdge::Right}) {
            const StackAnchorParams p = params(edge);
            const StackPlacement placed = placeStack(p);
            const double shelfInner = p.shelfGap + p.shelfThickness;

            switch (edge) {
            case DockEdge::Bottom:
                QCOMPARE(p.outputHeight - (placed.y + p.popupHeight), shelfInner + p.clearance);
                break;
            case DockEdge::Top:
                QCOMPARE(placed.y, shelfInner + p.clearance);
                break;
            case DockEdge::Left:
                QCOMPARE(placed.x, shelfInner + p.clearance);
                break;
            case DockEdge::Right:
                QCOMPARE(p.outputWidth - (placed.x + p.popupWidth), shelfInner + p.clearance);
                break;
            }
        }
    }

    void stackNearTheCornerIsPushedBackOnScreen()
    {
        StackAnchorParams p = params(DockEdge::Bottom);

        // A tile at the very start of the dock: centring the stack on it would
        // hang most of it off the left of the screen.
        p.restingTileCentre = 30.0;
        QCOMPARE(placeStack(p).x, p.screenMargin);

        // And the same at the other end.
        p.restingTileCentre = p.outputWidth - 30.0;
        QCOMPARE(placeStack(p).x, p.outputWidth - p.popupWidth - p.screenMargin);

        // A tile comfortably inside is not moved at all — the clamp must not be
        // doing anything when it has nothing to do.
        p.restingTileCentre = 960.0;
        QCOMPARE(placeStack(p).x, 960.0 - p.popupWidth / 2.0);
    }

    void stackWiderThanTheOutputPinsToTheLeadingMargin()
    {
        StackAnchorParams p = params(DockEdge::Bottom);
        p.popupWidth = p.outputWidth + 200.0;
        p.restingTileCentre = 960.0;

        // Nothing can make this fit. Pinning it to the leading margin at least
        // keeps its start visible; an inverted clamp range would be undefined.
        QCOMPARE(placeStack(p).x, p.screenMargin);
    }

    // --- The §11 defect ----------------------------------------------------

    void positionCorrectUnderMagnification()
    {
        DockGeometry geometry;
        configure(geometry, 8);

        const qreal resting = geometry.restingCentreOfRow(3);
        QVERIFY(resting > 0.0);

        // The pointer sits *near* tile 3, not on it. magnify() anchors the
        // pointed tile under the cursor, so a pointer exactly on the resting
        // centre leaves the drawn centre exactly there too — the displacement
        // this test is about only appears off-centre, which is also where the
        // pointer spends almost all of its time.
        geometry.setPointerPosition(resting - 30.0);
        const QVariantMap magnified = geometry.tileGeometry().at(3).toMap();
        const qreal drawnCentre =
            magnified.value(QStringLiteral("offset")).toReal() + magnified.value(QStringLiteral("size")).toReal() / 2.0;

        // The premise: with the pointer there, the drawn tile really is bigger
        // than it rests. Without this the test below would prove nothing.
        QVERIFY(magnified.value(QStringLiteral("size")).toReal() > 48.0);

        StackAnchorParams p = params(DockEdge::Bottom);
        p.restingTileCentre = resting;
        const StackPlacement anchored = placeStack(p);

        p.restingTileCentre = drawnCentre;
        const StackPlacement wrong = placeStack(p);

        // They differ, which is what makes anchoring to the right one matter.
        QVERIFY(std::abs(anchored.x - wrong.x) > 0.5);
    }

    /// The regression this whole task exists for: an open stack must not shift
    /// when the pointer moves. Reported against the reference platform as a
    /// 10–15 px jump on first pointer movement with magnification enabled.
    void noPositionalJumpOnPointerMove()
    {
        DockGeometry geometry;
        configure(geometry, 8);

        const int row = 3;
        const qreal opened = geometry.restingCentreOfRow(row);
        QVERIFY(opened > 0.0);

        StackAnchorParams p = params(DockEdge::Bottom);
        p.restingTileCentre = opened;
        const StackPlacement atOpen = placeStack(p);

        // Sweep the pointer the length of the dock, which is what the user does
        // immediately after clicking. Every step re-reads the anchor.
        bool anyMagnificationSeen = false;
        for (qreal pointer = 0.0; pointer <= 700.0; pointer += 7.0) {
            geometry.setPointerPosition(pointer);

            const QVariantMap drawn = geometry.tileGeometry().at(row).toMap();
            if (drawn.value(QStringLiteral("size")).toReal() > 48.5) {
                anyMagnificationSeen = true;
            }

            p.restingTileCentre = geometry.restingCentreOfRow(row);
            QCOMPARE(placeStack(p), atOpen);
        }

        // The sweep has to have actually magnified the tile at some point, or
        // the loop above proved only that nothing happened.
        QVERIFY2(anyMagnificationSeen, "the pointer sweep never magnified the tile");

        // And back to rest.
        geometry.setPointerPosition(-1.0);
        p.restingTileCentre = geometry.restingCentreOfRow(row);
        QCOMPARE(placeStack(p), atOpen);
    }

    void restingCentreIsUnmovedByThePointer()
    {
        DockGeometry geometry;
        configure(geometry, 6);

        std::vector<qreal> before;
        for (int row = 0; row < 6; ++row) {
            before.push_back(geometry.restingCentreOfRow(row));
        }

        for (qreal pointer : {0.0, 50.0, 120.0, 300.0, 480.0}) {
            geometry.setPointerPosition(pointer);
            for (int row = 0; row < 6; ++row) {
                QCOMPARE(geometry.restingCentreOfRow(row), before[std::size_t(row)]);
            }
        }
    }

    void restingCentreRefusesSeparatorsAndMissingRows()
    {
        DockGeometry geometry;
        configure(geometry, 5);
        geometry.setSeparatorRows({2});

        // A separator is a rule between regions, not a cell. Nothing opens from
        // it, and returning a plausible-looking number for one would place a
        // stack against something that is not there.
        QCOMPARE(geometry.restingCentreOfRow(2), -1.0);
        QCOMPARE(geometry.restingCentreOfRow(-1), -1.0);
        QCOMPARE(geometry.restingCentreOfRow(99), -1.0);
        QVERIFY(geometry.restingCentreOfRow(0) > 0.0);
        QVERIFY(geometry.restingCentreOfRow(4) > 0.0);
    }

    void stackFollowsTheTileWhenTheDockReflows()
    {
        DockGeometry geometry;
        configure(geometry, 4);

        StackAnchorParams p = params(DockEdge::Bottom);
        p.restingTileCentre = geometry.restingCentreOfRow(3);
        const StackPlacement before = placeStack(p);

        // The dock is resized, so every tile moves. An open stack has to follow
        // its own tile rather than stay where it was.
        //
        // Note what does *not* move a tile: appending one after it. The engine
        // publishes offsets from the strip's start, and centring the strip in
        // the surface is the view's arithmetic, not the engine's — so a stack
        // following a tile through *that* kind of reflow is a question for the
        // QML, and this asserts only the part the engine owns.
        geometry.setTileSize(72.0);
        p.restingTileCentre = geometry.restingCentreOfRow(3);
        const StackPlacement after = placeStack(p);

        QVERIFY(after != before);
        QCOMPARE(after.x, geometry.restingCentreOfRow(3) - p.popupWidth / 2.0);
    }
};

QTEST_MAIN(TestStackPositioning)
#include "test_stackpositioning.moc"
