import QtQuick
import QtTest
import org.kde.frappedock

/*
 * Drag-to-reorder, as a gesture.
 *
 * That the *model* reorders correctly is test_tilemodel_reorder's business.
 * What is checked here is the wire between pointer and model: that a press with
 * enough travel moves a row, that the strip reflows while the finger is still
 * down rather than on release, and that a drag does not also count as a click.
 *
 * The model is a ListModel with a moveTile() of its own — the same shape the
 * real TileModel exposes to QML. A real one needs a launcher backend and a
 * config file, neither of which a QML test has, and neither of which this test
 * is about.
 */
TestCase {
    id: testCase
    name: "Reorder"
    when: windowShown
    visible: true
    width: 800
    height: 400

    property int originalSpeed: 0

    function initTestCase() {
        testCase.originalSpeed = FrappeConfig.animationSpeed;
        // Reflow is checked by where tiles end up, not by how they get there.
        // Animation would make every position assertion a race.
        FrappeConfig.animationSpeed = 0;
    }

    function cleanupTestCase() {
        FrappeConfig.animationSpeed = testCase.originalSpeed;
    }

    Component {
        id: dockComponent
        Dock {}
    }

    Component {
        id: modelComponent
        ListModel {
            /// The one method Dock.qml asks the model for. Returning false for
            /// a rejected move matters: the dock uses it to decide whether the
            /// dragged row has actually changed index.
            function moveTile(from, to) {
                if (from < 0 || to < 0 || from >= count || to >= count || from === to) {
                    return false;
                }
                move(from, to, 1);
                return true;
            }
        }
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }

    function makeDock(withSeparator) {
        let model = createTemporaryObject(modelComponent, testCase);
        verify(model);

        model.append({ tileId: "alpha", name: "Alpha", iconName: "", kind: 0,
                       isRunning: false, windowCount: 0, isPinned: true });
        if (withSeparator) {
            model.append({ tileId: "sep", name: "", iconName: "", kind: 4,
                           isRunning: false, windowCount: 0, isPinned: true });
        }
        model.append({ tileId: "beta", name: "Beta", iconName: "", kind: 0,
                       isRunning: false, windowCount: 0, isPinned: true });
        model.append({ tileId: "gamma", name: "Gamma", iconName: "", kind: 0,
                       isRunning: false, windowCount: 0, isPinned: true });

        let dock = createTemporaryObject(dockComponent, testCase, { tileModel: model });
        verify(dock);
        return dock;
    }

    function tileAt(dock, row) {
        let tile = findChild(dock, "repeater").itemAt(row);
        verify(tile);
        return tile;
    }

    function order(dock) {
        let ids = [];
        for (let row = 0; row < dock.tileModel.count; ++row) {
            ids.push(dock.tileModel.get(row).tileId);
        }
        return ids;
    }

    /// Centre of the row'th tile, in the dock's coordinates.
    function centreOf(dock, row) {
        let tile = tileAt(dock, row);
        return tile.mapToItem(dock, tile.width / 2, tile.height / 2);
    }

    /// Regression: magnification was fed only by the HoverHandler, which stops
    /// updating once the drag takes the pointer grab. The strip froze at the
    /// position the drag began, so the magnified placements the drop target is
    /// read from no longer matched the pointer — the tile could land a cell away
    /// from the gap the user was looking at.
    function test_magnificationFollowsThePointerDuringADrag() {
        let dock = makeDock(false);
        let geometry = findChild(dock, "geometry");
        verify(geometry);
        geometry.magnificationEnabled = true;

        let start = centreOf(dock, 0);
        let middle = centreOf(dock, 1);
        let target = centreOf(dock, 2);

        // The first move is what starts the drag — a press alone is still a
        // click — so the pointer is only under the grab from here on.
        mousePress(dock, start.x, start.y);
        mouseMove(dock, middle.x, middle.y);
        compare(dock.draggingRow >= 0, true);

        let underway = geometry.pointerPosition;
        verify(underway >= 0);

        mouseMove(dock, target.x, target.y);
        verify(geometry.pointerPosition > underway);

        mouseRelease(dock, target.x, target.y);
    }

    /// The tile in the user's hand keeps its resting size. Sizing it from the
    /// magnification curve made it grow and shrink according to where the
    /// layout still thought it was, which is not where it is.
    function test_draggedTileIsNotMagnified() {
        let dock = makeDock(false);
        let geometry = findChild(dock, "geometry");
        verify(geometry);
        geometry.magnificationEnabled = true;

        let start = centreOf(dock, 0);
        let target = centreOf(dock, 2);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, target.x, target.y);
        compare(dock.draggingRow >= 0, true);

        // Held at rest while the pointer sits right on top of it — which is
        // where the curve peaks, so an unfixed tile is at its largest here.
        compare(tileAt(dock, dock.draggingRow).iconSize, dock.iconSize);

        mouseRelease(dock, target.x, target.y);
    }

    function test_dragToNextCellReorders() {
        let dock = makeDock(false);
        compare(order(dock), ["alpha", "beta", "gamma"]);

        let start = centreOf(dock, 0);
        let target = centreOf(dock, 1);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, target.x, target.y);
        mouseRelease(dock, target.x, target.y);

        compare(order(dock), ["beta", "alpha", "gamma"]);
        // State is cleared on release, or the next press would resume the last
        // drag.
        compare(dock.draggingRow, -1);
    }

    /*
     * Regression, reported from manual testing on 2026-09-03: the dock froze
     * while dragging a tile *over* the strip.
     *
     * A real pointer does not arrive one cell at a time. It delivers a stream
     * of moves, many of them at the same place, and the existing tests here all
     * jump a whole cell per move — so none of them could see what happens when
     * it stops. If the reflow that follows a move puts the pointer back over
     * the row it came from, the next move undoes it, and the drag ping-pongs
     * for as long as the finger is held still. Every swap writes the pinned
     * order to config, and every write reconfigures every layer surface.
     */
    function test_aStationaryDragDoesNotKeepReordering() {
        let dock = makeDock(false);
        let geometry = findChild(dock, "geometry");
        verify(geometry);
        // Magnification on: it is the reflow under the pointer that can put the
        // dragged row back where it started.
        geometry.magnificationEnabled = true;

        let start = centreOf(dock, 0);
        let target = centreOf(dock, 1);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, target.x, target.y);

        let settled = order(dock);
        let settledRow = dock.draggingRow;

        // The finger stops. The moves keep coming, as they do in a real
        // session — a pixel of jitter either way, no more.
        for (let i = 0; i < 24; ++i) {
            mouseMove(dock, target.x + (i % 2), target.y);
        }

        compare(order(dock), settled);
        compare(dock.draggingRow, settledRow);

        mouseRelease(dock, target.x, target.y);
    }

    function test_dragAcrossTheStripLandsAtTheEnd() {
        let dock = makeDock(false);

        let start = centreOf(dock, 0);
        let middle = centreOf(dock, 1);
        let end = centreOf(dock, 2);

        // Stepped, the way a real pointer arrives: the row is moved on the way
        // past rather than once on drop.
        mousePress(dock, start.x, start.y);
        mouseMove(dock, middle.x, middle.y);
        mouseMove(dock, end.x, end.y);
        mouseRelease(dock, end.x, end.y);

        compare(order(dock), ["beta", "gamma", "alpha"]);
    }

    function test_reflowIsLiveDuringTheDrag() {
        let dock = makeDock(false);

        let beta = tileAt(dock, 1);
        let betaBefore = beta.x;

        let start = centreOf(dock, 0);
        let target = centreOf(dock, 1);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, target.x, target.y);

        // Still held down: the neighbour has already moved out of the way, and
        // the model already says so.
        compare(order(dock), ["beta", "alpha", "gamma"]);
        verify(beta.x < betaBefore);

        // And the dragged tile is tracking the pointer rather than sitting in
        // the cell it came from.
        verify(dock.draggingRow >= 0);

        mouseRelease(dock, target.x, target.y);
    }

    function test_movementBelowThresholdIsAClickNotADrag() {
        let dock = makeDock(false);
        let clicked = createTemporaryObject(signalSpyComponent, testCase,
                                           { target: dock, signalName: "tileClicked" });
        verify(clicked.valid);

        let start = centreOf(dock, 0);

        mousePress(dock, start.x, start.y);
        // A pixel of jitter, well inside the drag threshold.
        mouseMove(dock, start.x + 1, start.y);
        mouseRelease(dock, start.x + 1, start.y);

        compare(order(dock), ["alpha", "beta", "gamma"]);
        compare(clicked.count, 1);
        compare(clicked.signalArguments[0][0], "alpha");
    }

    function test_dragDoesNotAlsoClick() {
        let dock = makeDock(false);
        let clicked = createTemporaryObject(signalSpyComponent, testCase,
                                           { target: dock, signalName: "tileClicked" });
        verify(clicked.valid);

        let start = centreOf(dock, 0);
        let target = centreOf(dock, 1);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, target.x, target.y);
        mouseRelease(dock, target.x, target.y);

        // Reordering an application must not also launch it.
        compare(clicked.count, 0);
    }

    /// A separator is a rule, not a cell: there is nothing behind it to move,
    /// and dragging one would reorder the regions themselves.
    ///
    /// Where a tile dragged *across* a separator may land is a different
    /// question — that is the region validity matrix in §4.4, and until it
    /// exists the strip is one flat sequence.
    function test_separatorCannotBeDragged() {
        let dock = makeDock(true);
        compare(order(dock), ["alpha", "sep", "beta", "gamma"]);

        let separator = centreOf(dock, 1);
        let far = centreOf(dock, 3);

        mousePress(dock, separator.x, separator.y);
        mouseMove(dock, far.x, far.y);
        mouseRelease(dock, far.x, far.y);

        compare(order(dock), ["alpha", "sep", "beta", "gamma"]);
        compare(dock.draggingRow, -1);
    }
}
