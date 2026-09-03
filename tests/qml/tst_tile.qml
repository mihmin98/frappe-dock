import QtQuick
import QtTest
import org.kde.frappedock

/*
 * Rendering, interaction, and — most importantly — that the proportion model
 * survives contact with QML.
 */
TestCase {
    id: testCase
    name: "Tile"
    when: windowShown
    visible: true
    width: 800
    height: 400

    // Snapshotted in initTestCase, not bound: a binding would track the very
    // changes these tests make and cleanup would restore nothing.
    property int originalTileSize: 0

    function initTestCase() {
        testCase.originalTileSize = FrappeConfig.tileSize;
    }

    Component {
        id: dockComponent
        Dock {}
    }

    function cleanup() {
        FrappeConfig.tileSize = testCase.originalTileSize;
    }

    function test_tileCountMatchesModelRowCount() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "a", name: "A", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "b", name: "B", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "c", name: "C", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        verify(dock);
        let repeater = findChild(dock, "repeater");
        verify(repeater);
        compare(repeater.count, 3);
    }

    function test_clickEmitsTileClickedWithCorrectId() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "alpha", name: "Alpha", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "beta", name: "Beta", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        verify(dock);
        let spy = createTemporaryObject(signalSpyComponent, testCase,
                                        { target: dock, signalName: "tileClicked" });
        verify(spy.valid);

        let repeater = findChild(dock, "repeater");
        // The second tile, so an off-by-one between index and id would show up.
        let tile = repeater.itemAt(1);
        verify(tile);
        mouseClick(tile);

        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "beta");
        compare(spy.signalArguments[0][1], Qt.LeftButton);
        compare(spy.signalArguments[0][2], Qt.NoModifier);
    }

    function test_emptyModelRendersEmptyShelfWithoutError() {
        let dock = createTemporaryObject(dockComponent, testCase, { tileModel: [] });
        verify(dock);
        let repeater = findChild(dock, "repeater");
        compare(repeater.count, 0);

        let shelf = findChild(dock, "shelf");
        verify(shelf);
        // Still a shelf, just an empty one: padding on both sides and nothing
        // between them.
        compare(shelf.height, dock.thickness);
        fuzzyCompare(shelf.width, 2 * dock.gap, 0.001);
    }

    function test_proportionsFollowTileSize_data() {
        return [
            { tag: "min", S: 24 },
            { tag: "max", S: 128 },
            { tag: "reference", S: 47 },  // the reference dock fits S ~= 46.5
        ];
    }

    /*
     * The test that catches a hardcoded dimension. A dock at S = 24 must be a
     * uniform scale of a dock at S = 128; a literal left anywhere breaks that at
     * one end or the other.
     *
     * Tolerance is 1 % of the expected value, matching the ~3 % error bar on the
     * measurements these ratios came from (plan.md Part 0). Do not tighten it.
     */
    function test_proportionsFollowTileSize(data) {
        FrappeConfig.tileSize = data.S;

        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "a", name: "A", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "b", name: "B", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        verify(dock);

        let S = data.S;
        let expectedGap = S / 3;
        let expectedThickness = S + 2 * expectedGap;
        let expectedPitch = S + expectedGap;
        let expectedRadius = 0.28 * expectedThickness;

        let shelf = findChild(dock, "shelf");
        let repeater = findChild(dock, "repeater");
        let first = repeater.itemAt(0);
        let last = repeater.itemAt(1);

        fuzzyCompare(dock.gap, expectedGap, expectedGap * 0.01);
        fuzzyCompare(shelf.height, expectedThickness, expectedThickness * 0.01);
        fuzzyCompare(shelf.radius, expectedRadius, expectedRadius * 0.01);

        // Shelf padding, all four sides, equals the inter-icon gap. Measured
        // between the shelf and the tiles the engine actually placed, so a
        // padding that is declared but not applied fails here.
        let paddingLeft = first.x;
        let paddingRight = shelf.width - (last.x + last.width);
        let paddingTop = first.y;
        let paddingBottom = shelf.height - (first.y + first.height);
        fuzzyCompare(paddingLeft, expectedGap, expectedGap * 0.01);
        fuzzyCompare(paddingRight, expectedGap, expectedGap * 0.01);
        fuzzyCompare(paddingTop, expectedGap, expectedGap * 0.01);
        fuzzyCompare(paddingBottom, expectedGap, expectedGap * 0.01);

        // The shelf floats S/9 clear of the screen edge. It is the surface's
        // edge that stands in for the screen's here: the dock item spans the
        // output on a real surface, and a shelf sitting flush is the usual
        // symptom of the gap being lost to the reserved zone.
        let expectedScreenGap = S / 9;
        let screenGap = dock.height - (shelf.y + shelf.height);
        fuzzyCompare(screenGap, expectedScreenGap, expectedScreenGap * 0.01);

        // Cell pitch, measured between two laid-out tiles.
        let pitch = repeater.itemAt(1).x - repeater.itemAt(0).x;
        fuzzyCompare(pitch, expectedPitch, expectedPitch * 0.01);
    }

    /*
     * The reference dock, cross-checked against
     * the reference screenshot: at S ~= 46.5 the measured
     * shelf is 77.5 pt thick with a 62 pt cell pitch.
     */
    function test_referenceGeometry() {
        FrappeConfig.tileSize = 47;

        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "a", name: "A", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "b", name: "B", iconName: "", kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        let shelf = findChild(dock, "shelf");
        let repeater = findChild(dock, "repeater");

        fuzzyCompare(shelf.height, 77.5, 1.0);
        fuzzyCompare(repeater.itemAt(1).x - repeater.itemAt(0).x, 62.0, 1.0);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
