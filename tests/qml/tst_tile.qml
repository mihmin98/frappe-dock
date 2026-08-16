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

    property int originalTileSize: FrappeConfig.tileSize

    Component {
        id: dockComponent
        Dock {}
    }

    function cleanup() {
        FrappeConfig.tileSize = testCase.originalTileSize;
    }

    function test_tileCountMatchesModelRowCount() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "a", name: "A", iconName: "", kind: 0 },
                                                        { tileId: "b", name: "B", iconName: "", kind: 0 },
                                                        { tileId: "c", name: "C", iconName: "", kind: 0 } ] });
        verify(dock);
        let repeater = findChild(dock, "repeater");
        verify(repeater);
        compare(repeater.count, 3);
    }

    function test_clickEmitsLaunchSignalWithCorrectId() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "alpha", name: "Alpha", iconName: "", kind: 0 },
                                                        { tileId: "beta", name: "Beta", iconName: "", kind: 0 } ] });
        verify(dock);
        let spy = createTemporaryObject(signalSpyComponent, testCase,
                                        { target: dock, signalName: "launchRequested" });
        verify(spy.valid);

        let repeater = findChild(dock, "repeater");
        // The second tile, so an off-by-one between index and id would show up.
        let tile = repeater.itemAt(1);
        verify(tile);
        mouseClick(tile);

        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "beta");
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
                                         { tileModel: [ { tileId: "a", name: "A", iconName: "", kind: 0 },
                                                        { tileId: "b", name: "B", iconName: "", kind: 0 } ] });
        verify(dock);

        let S = data.S;
        let expectedGap = S / 3;
        let expectedThickness = S + 2 * expectedGap;
        let expectedPitch = S + expectedGap;
        let expectedRadius = 0.28 * expectedThickness;

        let shelf = findChild(dock, "shelf");
        let row = findChild(dock, "row");
        let repeater = findChild(dock, "repeater");

        fuzzyCompare(dock.gap, expectedGap, expectedGap * 0.01);
        fuzzyCompare(shelf.height, expectedThickness, expectedThickness * 0.01);
        fuzzyCompare(row.spacing, expectedGap, expectedGap * 0.01);
        fuzzyCompare(shelf.radius, expectedRadius, expectedRadius * 0.01);

        // Shelf padding, all four sides, equals the inter-icon gap. Measured off
        // the actual laid-out row rather than read back off a property, so a
        // padding that is declared but not applied fails here.
        let paddingLeft = row.x;
        let paddingRight = shelf.width - (row.x + row.width);
        let paddingTop = row.y;
        let paddingBottom = shelf.height - (row.y + row.height);
        fuzzyCompare(paddingLeft, expectedGap, expectedGap * 0.01);
        fuzzyCompare(paddingRight, expectedGap, expectedGap * 0.01);
        fuzzyCompare(paddingTop, expectedGap, expectedGap * 0.01);
        fuzzyCompare(paddingBottom, expectedGap, expectedGap * 0.01);

        // Cell pitch, measured between two laid-out tiles.
        let pitch = repeater.itemAt(1).x - repeater.itemAt(0).x;
        fuzzyCompare(pitch, expectedPitch, expectedPitch * 0.01);
    }

    /*
     * The reference dock, cross-checked against
     * .reference-screenshots/26-Tahoe-Desktop.png: at S ~= 46.5 the measured
     * shelf is 77.5 pt thick with a 62 pt cell pitch.
     */
    function test_referenceGeometry() {
        FrappeConfig.tileSize = 47;

        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "a", name: "A", iconName: "", kind: 0 },
                                                        { tileId: "b", name: "B", iconName: "", kind: 0 } ] });
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
