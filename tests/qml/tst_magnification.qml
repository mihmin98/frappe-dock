import QtQuick
import QtTest
import org.kde.frappedock

/*
 * That what is drawn is what the engine said, at every pointer position.
 *
 * The engine's own behaviour — the curve, the anchoring, the compression — is
 * tested headless in test_magnification.cpp and test_layout.cpp, and is not
 * retested here. What only QML can be asked is whether the view is a faithful
 * reading of the engine's output, or whether it has quietly grown a second way
 * of deciding where a tile goes. A fallback that disagrees with the engine is
 * how layout and hit regions drift apart (plan.md §3.4), and it is invisible
 * until you compare the two.
 *
 * Animation is switched off throughout: these are assertions about layout, and
 * a tile caught mid-transition is neither the old layout nor the new one.
 */
TestCase {
    id: testCase
    name: "Magnification"
    when: windowShown
    visible: true
    width: 1000
    height: 400

    // Snapshotted rather than bound: a binding would track the changes these
    // tests make, and cleanup would restore nothing.
    property int originalTileSize: 0
    property int originalSpeed: 0
    property bool originalMagnification: false
    property real originalFactor: 0
    property int originalPosition: 0

    function initTestCase() {
        testCase.originalTileSize = FrappeConfig.tileSize;
        testCase.originalSpeed = FrappeConfig.animationSpeed;
        testCase.originalMagnification = FrappeConfig.magnificationEnabled;
        testCase.originalFactor = FrappeConfig.magnificationFactor;
        testCase.originalPosition = FrappeConfig.position;
    }

    function init() {
        FrappeConfig.animationSpeed = 0;
        FrappeConfig.magnificationEnabled = true;
        FrappeConfig.tileSize = 48;
        FrappeConfig.magnificationFactor = testCase.originalFactor;
        FrappeConfig.position = testCase.originalPosition;
    }

    function cleanupTestCase() {
        FrappeConfig.tileSize = testCase.originalTileSize;
        FrappeConfig.animationSpeed = testCase.originalSpeed;
        FrappeConfig.magnificationEnabled = testCase.originalMagnification;
        FrappeConfig.magnificationFactor = testCase.originalFactor;
        FrappeConfig.position = testCase.originalPosition;
    }

    function model(count) {
        let rows = [];
        for (let i = 0; i < count; ++i) {
            rows.push({ tileId: "t" + i, name: "T" + i, iconName: "",
                        kind: 0, isRunning: false, windowCount: 0 });
        }
        return rows;
    }

    Component {
        id: dockComponent
        Dock {}
    }

    function makeDock(count) {
        let dock = createTemporaryObject(dockComponent, testCase, { tileModel: model(count) });
        verify(dock);
        return dock;
    }

    /// Every tile, checked against the entry the engine published for it. This
    /// is the assertion the whole file exists for; the rest of the tests are
    /// about the states it has to hold in.
    function verifyTilesMatchEngine(dock, context) {
        let repeater = findChild(dock, "repeater");
        let geometry = findChild(dock, "geometry");

        compare(geometry.tileGeometry.length, repeater.count, context + ": one entry per row");

        for (let i = 0; i < repeater.count; ++i) {
            let tile = repeater.itemAt(i);
            let placement = geometry.tileGeometry[i];
            // Tiles are drawn inside the shelf, and the shelf itself moves with
            // the strip, so the engine's offset is read relative to it.
            fuzzyCompare(tile.x, placement.offset - geometry.shelfStart, 0.001,
                         context + ": tile " + i + " drawn where the engine put it");
            fuzzyCompare(tile.width, placement.size, 0.001,
                         context + ": tile " + i + " drawn at the engine's size");
        }
    }

    function test_restingLayoutMatchesEngine() {
        let dock = makeDock(6);
        verifyTilesMatchEngine(dock, "at rest");

        // At rest every tile is the configured size, whatever the engine is
        // capable of doing to them.
        let repeater = findChild(dock, "repeater");
        for (let i = 0; i < repeater.count; ++i) {
            fuzzyCompare(repeater.itemAt(i).width, dock.iconSize, 0.001);
        }
    }

    function test_pointerAtCentreMagnifiesTheTileUnderIt() {
        let dock = makeDock(7);
        let repeater = findChild(dock, "repeater");
        let shelf = findChild(dock, "shelf");

        let target = repeater.itemAt(3);
        let restingSize = target.width;
        // In the dock's coordinates, not the shelf's: the shelf moves when the
        // strip grows, so a position measured inside it does not stay put.
        let pointer = shelf.x + target.x + target.width / 2;
        mouseMove(shelf, target.x + target.width / 2, shelf.height / 2);

        verify(target.width > restingSize, "the pointed tile grew");
        verifyTilesMatchEngine(dock, "pointer at centre");

        // Its neighbours grew less, and further out less again: the falloff is
        // the engine's, but that it reaches the *view* monotonically is not
        // something the engine can be asked about.
        verify(repeater.itemAt(2).width < target.width);
        verify(repeater.itemAt(1).width < repeater.itemAt(2).width);
        verify(repeater.itemAt(4).width < target.width);
        verify(repeater.itemAt(5).width < repeater.itemAt(4).width);

        // And it is still under the pointer, which is the property that makes
        // clicking a magnified tile land on the tile that was pointed at. The
        // engine guarantees this in its own coordinates; that the *drawn* tile
        // also covers the pointer is what the view can get wrong.
        let drawnStart = shelf.x + target.x;
        verify(drawnStart <= pointer && pointer <= drawnStart + target.width,
               "the pointed tile is still under the pointer");

        // And the hit test agrees with what was drawn — the two readings that
        // §3.4 exists to keep from drifting apart.
        let geometry = findChild(dock, "geometry");
        compare(geometry.rowAt(pointer - dock.restingOrigin), 3);
    }

    function test_pointerAtTheEdgeKeepsTheStripOnTheShelf() {
        let dock = makeDock(7);
        let repeater = findChild(dock, "repeater");
        let shelf = findChild(dock, "shelf");
        let geometry = findChild(dock, "geometry");

        let first = repeater.itemAt(0);
        let restingStart = first.x;

        mouseMove(shelf, first.x + first.width / 2, shelf.height / 2);

        verify(first.width > dock.iconSize, "the first tile grew");
        verifyTilesMatchEngine(dock, "pointer at the leading edge");

        // Magnification anchors on the pointer, which lets the strip slide off
        // the start of the shelf. Rebased, it must not: a tile drawn at a
        // negative offset is a tile outside the shelf.
        fuzzyCompare(first.x, restingStart, 0.001, "the strip still starts at the padding");

        let last = repeater.itemAt(repeater.count - 1);
        verify(last.x + last.width <= shelf.width + 0.001, "the strip still ends on the shelf");
        verify(shelf.width >= geometry.shelfLength - 0.001, "the shelf grew to hold it");
    }

    function test_pointerBeyondTheDockRests() {
        let dock = makeDock(6);
        let repeater = findChild(dock, "repeater");
        let shelf = findChild(dock, "shelf");

        let resting = [];
        for (let i = 0; i < repeater.count; ++i) {
            resting.push({ x: repeater.itemAt(i).x, size: repeater.itemAt(i).width });
        }

        mouseMove(shelf, shelf.width / 2, shelf.height / 2);
        verify(repeater.itemAt(3).width > dock.iconSize, "magnified while hovered");

        // Off the shelf entirely. The dock is exactly shelf-sized here, so the
        // move has to be made against the test case rather than against it.
        mouseMove(testCase, testCase.width - 1, testCase.height - 1);

        for (let i = 0; i < repeater.count; ++i) {
            fuzzyCompare(repeater.itemAt(i).x, resting[i].x, 0.001,
                         "tile " + i + " back to its resting position");
            fuzzyCompare(repeater.itemAt(i).width, resting[i].size, 0.001,
                         "tile " + i + " back to its resting size");
        }
        verifyTilesMatchEngine(dock, "pointer away");
    }

    /// Sweeping rather than sampling three positions: a disagreement between
    /// the view and the engine need not show up at a round number.
    function test_viewTracksTheEngineAcrossASweep() {
        let dock = makeDock(8);
        let shelf = findChild(dock, "shelf");

        for (let x = 0; x < shelf.width; x += 7) {
            mouseMove(shelf, x, shelf.height / 2);
            verifyTilesMatchEngine(dock, "pointer at " + x);
        }
    }

    /// With magnification off the pointer must change nothing at all — the
    /// resting layout is what the reference geometry fixes, and it has to
    /// survive the magnification code being in the path.
    function test_magnificationOffIgnoresThePointer() {
        FrappeConfig.magnificationEnabled = false;

        let dock = makeDock(6);
        let repeater = findChild(dock, "repeater");
        let shelf = findChild(dock, "shelf");

        mouseMove(shelf, shelf.width / 2, shelf.height / 2);

        for (let i = 0; i < repeater.count; ++i) {
            fuzzyCompare(repeater.itemAt(i).width, dock.iconSize, 0.001);
        }
        verifyTilesMatchEngine(dock, "magnification off");
    }

    /*
     * Regression, reported during the §3.5.5 tuning pass: at the maximum peak
     * size the icons were cut off by the dock's own edges. Two causes, and the
     * second is the one no amount of extra room would have fixed —
     *
     *   1. The surface was sized to the shelf, so a tile taller than the shelf
     *      had nowhere to be drawn.
     *   2. Tiles were centred across the shelf, so they grew *both* ways:
     *      half the growth headed straight into the screen edge, where there
     *      is no room to be had at any surface thickness.
     *
     * A magnified tile keeps its outer edge on the resting icon's line and
     * grows away from the screen edge, which is what these check.
     */
    function test_magnifiedTilesAreNotClipped_data() {
        return [
            { tag: "bottom", position: ConfigFacade.Bottom },
            { tag: "left", position: ConfigFacade.Left },
            { tag: "right", position: ConfigFacade.Right },
        ];
    }

    function test_magnifiedTilesAreNotClipped(data) {
        FrappeConfig.position = data.position;
        FrappeConfig.magnificationFactor = 3.0; // the schema's maximum

        let dock = makeDock(7);
        let repeater = findChild(dock, "repeater");
        let shelf = findChild(dock, "shelf");
        let horizontal = data.position === ConfigFacade.Bottom;

        let target = repeater.itemAt(3);
        // The resting outer edge: the side of the tile facing the screen edge.
        let restingOuter = horizontal
            ? shelf.y + target.y + target.height
            : (data.position === ConfigFacade.Left ? shelf.x + target.x
                                                   : shelf.x + target.x + target.width);

        mouseMove(shelf, horizontal ? target.x + target.width / 2 : shelf.width / 2,
                         horizontal ? shelf.height / 2 : target.y + target.height / 2);

        verify(target.width > dock.iconSize || target.height > dock.iconSize,
               "the tile magnified at all");

        // 1. It fits on the surface.
        let top = horizontal ? shelf.y + target.y : shelf.x + target.x;
        let bottom = horizontal ? shelf.y + target.y + target.height
                                : shelf.x + target.x + target.width;
        let extent = horizontal ? dock.height : dock.width;
        verify(top >= -0.001, "the tile does not run off the inner edge (" + top + ")");
        verify(bottom <= extent + 0.001,
               "the tile does not run off the outer edge (" + bottom + " > " + extent + ")");

        // 2. It grew away from the screen edge, not across it: the outer edge
        //    has not moved.
        let outer = data.position === ConfigFacade.Left ? top : bottom;
        fuzzyCompare(outer, restingOuter, 0.001, "the tile grew away from the screen edge");
    }

    /// The shelf is normative geometry and does not grow with the peak: the
    /// surface grows instead. A shelf that thickened with the magnification
    /// setting would break the proportion model.
    function test_theShelfDoesNotGrowWithThePeak() {
        FrappeConfig.magnificationFactor = 3.0;

        let dock = makeDock(7);
        let shelf = findChild(dock, "shelf");

        fuzzyCompare(shelf.height, dock.thickness, 0.001);
        verify(dock.height > shelf.height, "the surface is taller than the shelf");
    }

    /// A separator is a rule in a gap, not a cell. It must not magnify, and the
    /// tiles either side of it must still be where the engine says.
    function test_separatorsDoNotMagnify() {
        let rows = model(3);
        rows.splice(1, 0, { tileId: "sep", name: "", iconName: "",
                            kind: TileKind.Separator, isRunning: false, windowCount: 0 });

        let dock = createTemporaryObject(dockComponent, testCase, { tileModel: rows });
        let repeater = findChild(dock, "repeater");
        let shelf = findChild(dock, "shelf");

        let separator = repeater.itemAt(1);
        let restingWidth = separator.width;

        mouseMove(shelf, separator.x + separator.width / 2, shelf.height / 2);

        fuzzyCompare(separator.width, restingWidth, 0.001, "the separator kept its size");
    }
}
