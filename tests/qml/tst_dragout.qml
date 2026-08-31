import QtQuick
import QtTest
import org.kde.frappedock

/*
 * Drag out to remove, as a gesture.
 *
 * What unpinning does to configuration is test_dragout's business. What is
 * checked here is the part that only exists on screen: that the affordance
 * appears exactly when releasing would remove, that dragging back inside takes
 * it away again, and that a release short of the threshold is an ordinary
 * reorder.
 */
TestCase {
    id: testCase
    name: "DragOut"
    when: windowShown
    visible: true
    width: 800
    height: 400

    property int originalSpeed: 0

    function initTestCase() {
        testCase.originalSpeed = FrappeConfig.animationSpeed;
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
            /// The two methods Dock.qml asks a model for. `unpinTile` records
            /// what it was asked to remove rather than only doing it, so a test
            /// can tell "removed the right row" from "removed a row".
            property var unpinned: []

            function moveTile(from, to) {
                if (from < 0 || to < 0 || from >= count || to >= count || from === to) {
                    return false;
                }
                move(from, to, 1);
                return true;
            }

            function unpinTile(row) {
                if (row < 0 || row >= count || !get(row).isPinned) {
                    return false;
                }
                unpinned = unpinned.concat([get(row).tileId]);
                remove(row, 1);
                return true;
            }
        }
    }

    function makeDock(pinnedStates) {
        let model = createTemporaryObject(modelComponent, testCase);
        verify(model);

        let names = ["alpha", "beta", "gamma"];
        for (let i = 0; i < names.length; ++i) {
            model.append({ tileId: names[i], name: names[i], iconName: "", kind: 0,
                           isRunning: false, windowCount: 0,
                           isPinned: pinnedStates ? pinnedStates[i] : true });
        }

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

    function centreOf(dock, row) {
        let tile = tileAt(dock, row);
        return tile.mapToItem(dock, tile.width / 2, tile.height / 2);
    }

    function affordance(dock) {
        let item = findChild(dock, "removeAffordance");
        verify(item);
        return item;
    }

    /// A point out past the threshold, away from the screen edge. The dock is
    /// horizontal in these tests, so "out" is upwards.
    function outsidePoint(dock, row) {
        let centre = centreOf(dock, row);
        return Qt.point(centre.x,
                        dock.height - dock.thickness - dock.removeThreshold - dock.iconSize);
    }

    function test_affordanceAppearsPastThresholdAndNotBefore() {
        let dock = makeDock();
        verify(!affordance(dock).visible);

        let start = centreOf(dock, 0);
        mousePress(dock, start.x, start.y);

        // Lifted off the shelf, but not yet a tile's width clear of it.
        let shortOfIt = dock.height - dock.thickness - dock.removeThreshold / 2;
        mouseMove(dock, start.x, shortOfIt);
        verify(!affordance(dock).visible);
        verify(!dock.dragWillRemove);

        let out = outsidePoint(dock, 0);
        mouseMove(dock, out.x, out.y);
        verify(affordance(dock).visible);
        verify(dock.dragWillRemove);

        mouseRelease(dock, out.x, out.y);
    }

    function test_affordanceDisappearsWhenDraggedBackIn() {
        let dock = makeDock();

        let start = centreOf(dock, 0);
        let out = outsidePoint(dock, 0);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, out.x, out.y);
        verify(affordance(dock).visible);

        // Back onto the shelf: the gesture is called off by reversing it, so
        // there is nothing else for the user to do or undo.
        mouseMove(dock, start.x, start.y);
        verify(!affordance(dock).visible);
        verify(!dock.dragWillRemove);

        mouseRelease(dock, start.x, start.y);
        compare(dock.tileModel.unpinned.length, 0);
        compare(order(dock), ["alpha", "beta", "gamma"]);
    }

    function test_releasePastThresholdUnpins() {
        let dock = makeDock();

        let start = centreOf(dock, 0);
        let out = outsidePoint(dock, 0);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, out.x, out.y);
        mouseRelease(dock, out.x, out.y);

        compare(dock.tileModel.unpinned, ["alpha"]);
        compare(order(dock), ["beta", "gamma"]);
        compare(dock.draggingRow, -1);
        verify(!affordance(dock).visible);
    }

    function test_releaseInsideThresholdDoesNotUnpin() {
        let dock = makeDock();

        let start = centreOf(dock, 0);
        let target = centreOf(dock, 1);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, target.x, target.y);
        mouseRelease(dock, target.x, target.y);

        // An ordinary reorder, and nothing removed.
        compare(dock.tileModel.unpinned.length, 0);
        compare(order(dock), ["beta", "alpha", "gamma"]);
    }

    /// Only a pinned tile has something to remove. A running but unpinned
    /// application is in the dock because it is running, and dragging it out
    /// cannot un-run it.
    function test_unpinnedTileOffersNoAffordance() {
        let dock = makeDock([false, true, true]);

        let start = centreOf(dock, 0);
        let out = outsidePoint(dock, 0);

        mousePress(dock, start.x, start.y);
        mouseMove(dock, out.x, out.y);

        verify(!affordance(dock).visible);
        verify(!dock.dragWillRemove);

        mouseRelease(dock, out.x, out.y);
        compare(dock.tileModel.unpinned.length, 0);
        compare(dock.tileModel.count, 3);
    }

    /// While the tile is out, the strip must not reflow behind it: dragging
    /// back in to cancel has to return it to the cell it came from.
    function test_stripDoesNotReflowWhileTheTileIsOut() {
        let dock = makeDock();

        let start = centreOf(dock, 0);
        let over = centreOf(dock, 2);

        mousePress(dock, start.x, start.y);
        // Out past the threshold, then along to the far end of the dock.
        mouseMove(dock, start.x, dock.height - dock.thickness - dock.removeThreshold - dock.iconSize);
        mouseMove(dock, over.x, dock.height - dock.thickness - dock.removeThreshold - dock.iconSize);
        compare(order(dock), ["alpha", "beta", "gamma"]);

        // Back down onto the shelf at the far end: now it is a reorder again.
        mouseMove(dock, over.x, over.y);
        compare(order(dock), ["beta", "gamma", "alpha"]);

        mouseRelease(dock, over.x, over.y);
        compare(dock.tileModel.unpinned.length, 0);
    }
}
