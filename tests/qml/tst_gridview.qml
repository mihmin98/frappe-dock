import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * The stack Grid view: what it shows, that it scrolls, and that it drills down
 * and comes back.
 *
 * The model is a real StackModel over a fake listing (StackFixture), because
 * GridView needs an actual QAbstractItemModel for its rows while the same
 * object also answers the navigation questions.
 */
Item {
    id: root

    width: 600
    height: 500

    Component {
        id: gridComponent

        StackGrid {
            cellSize: 48
            columns: 4
            visibleRows: 3
        }
    }

    TestCase {
        id: testCase

        name: "StackGrid"
        when: windowShown

        function makeGrid(root_path) {
            let model = StackFixture.create(root_path);
            let grid = gridComponent.createObject(root, { model: model });
            verify(grid, "grid created");
            return grid;
        }

        function ready(grid) {
            StackFixture.setStatus(grid.model, 2); // Ready
            StackFixture.notifyChange(grid.model);
        }

        function gridView(grid) {
            // The GridView is the only child of that type; the test reaches it
            // by type rather than by objectName so the view keeps its internals
            // to itself.
            for (let i = 0; i < grid.children.length; ++i) {
                if (grid.children[i] instanceof GridView) {
                    return grid.children[i];
                }
            }
            return null;
        }

        function test_itemCountMatchesModel() {
            let grid = makeGrid("/stack/Root");
            StackFixture.addDir(grid.model, "alpha");
            StackFixture.addFile(grid.model, "notes.txt");
            StackFixture.addFile(grid.model, "report.pdf");
            ready(grid);

            compare(grid.model.count, 3);
            compare(gridView(grid).count, 3);

            // Removing one takes a cell away rather than leaving a hole.
            StackFixture.removeEntry(grid.model, "notes.txt");
            StackFixture.notifyChange(grid.model);
            compare(gridView(grid).count, 2);

            grid.destroy();
        }

        function test_gridDoesNotWidenWithItsContents() {
            let grid = makeGrid("/stack/Root");
            StackFixture.addFiles(grid.model, 4);
            ready(grid);
            let narrow = grid.implicitWidth;

            StackFixture.addFiles(grid.model, 60);
            StackFixture.notifyChange(grid.model);

            // A stack that widened with its contents would change shape every
            // time a file was added to the folder.
            compare(grid.implicitWidth, narrow);
            grid.destroy();
        }

        function test_scrollingWorks() {
            let grid = makeGrid("/stack/Root");
            // 40 entries over 4 columns is 10 rows against 3 visible.
            StackFixture.addFiles(grid.model, 40);
            ready(grid);

            let view = gridView(grid);
            // GridView fills in contentHeight on its next polish pass, not when
            // the rows arrive, so anything asking about it has to let a frame go
            // by first.
            waitForRendering(view);
            verify(view.contentHeight > view.height,
                   "content taller than the viewport, or there is nothing to scroll");
            compare(view.contentY, 0);

            view.contentY = view.cellHeight * 2;
            compare(view.contentY, view.cellHeight * 2);

            // The last row has to be reachable, not merely drawn past the edge.
            view.positionViewAtEnd();
            verify(view.contentY > 0, "view scrolled to the end");
            grid.destroy();
        }

        function test_drillDownAndReturn() {
            let grid = makeGrid("/stack/Root");
            StackFixture.addDir(grid.model, "inner");
            StackFixture.addFile(grid.model, "outer.txt");
            ready(grid);

            compare(grid.model.canGoUp, false);
            compare(gridView(grid).count, 2);

            // Entering the folder: the fake clears its listing the way a new
            // directory listing would, then reports the child's contents.
            verify(grid.model.enterFolder(0), "row 0 is the folder");
            compare(grid.model.path, "/stack/Root/inner");
            compare(grid.model.canGoUp, true);

            StackFixture.addFile(grid.model, "deep.txt");
            ready(grid);
            compare(gridView(grid).count, 1);

            verify(grid.model.goUp(), "back up");
            compare(grid.model.path, "/stack/Root");
            compare(grid.model.canGoUp, false);
            grid.destroy();
        }

        function test_scrollPositionResetsOnNavigation() {
            let grid = makeGrid("/stack/Root");
            // Named to sort ahead of the files, so row 0 is the folder.
            StackFixture.addDir(grid.model, "aaa-inner");
            StackFixture.addFiles(grid.model, 40);
            ready(grid);

            let view = gridView(grid);
            waitForRendering(view);
            view.positionViewAtEnd();
            verify(view.contentY > 0, "scrolled away from the top");

            verify(grid.model.enterFolder(0), "row 0 is the folder");
            StackFixture.addFile(grid.model, "deep.txt");
            ready(grid);

            // Carrying the old offset into a shorter folder parks the view past
            // the end of its contents, which reads as an empty folder.
            compare(view.contentY, 0);
            grid.destroy();
        }

        function test_breadcrumbFollowsTheCurrentFolder() {
            let grid = makeGrid("/stack/Root");
            compare(grid.model.trail.length, 1);
            compare(grid.model.trail[0].name, "Root");

            grid.model.path = "/stack/Root/a/b";
            compare(grid.model.trail.length, 3);
            compare(grid.model.trail[2].name, "b");
            grid.destroy();
        }

        function test_loadingStateIsShownAndHidesTheRows() {
            let grid = makeGrid("/stack/Root");
            StackFixture.setStatus(grid.model, 1); // Loading
            StackFixture.notifyChange(grid.model);

            compare(grid.loading, true);
            // A stale listing under a loading message is worse than no listing.
            compare(gridView(grid).count, 0);

            StackFixture.addFile(grid.model, "arrived.txt");
            ready(grid);
            compare(grid.loading, false);
            compare(gridView(grid).count, 1);
            grid.destroy();
        }

        function test_failedAndEmptyStatesAreDistinct() {
            let grid = makeGrid("/stack/Root");
            ready(grid);
            compare(grid.empty, true);
            compare(grid.failed, false);

            StackFixture.setStatus(grid.model, 3); // Failed
            StackFixture.notifyChange(grid.model);
            compare(grid.failed, true);
            compare(grid.empty, false);
            grid.destroy();
        }

        function test_proportionsFollowCellSize() {
            let grid = makeGrid("/stack/Root");
            StackFixture.addFiles(grid.model, 8);
            ready(grid);

            let small = { w: grid.implicitWidth, h: grid.implicitHeight, pitch: grid.cellPitch };
            grid.cellSize = 96;

            // A grid at S = 96 is a uniform scale of one at S = 48. The label
            // box is sized from the font, which also scales with S, so the whole
            // thing doubles rather than only the artwork.
            fuzzyCompare(grid.cellPitch, small.pitch * 2, small.pitch * 0.01);
            fuzzyCompare(grid.implicitWidth, small.w * 2, small.w * 0.01);
            // 1%, matching the tolerance the geometry tests use: font metrics
            // round to whole pixels, so the label box does not scale exactly.
            fuzzyCompare(grid.implicitHeight, small.h * 2, small.h * 0.01);
            grid.destroy();
        }

        function test_gapIsTheSameConstantAsTheDockUses() {
            let grid = makeGrid("/stack/Root");
            // S/3, the dock's inter-icon gap and shelf padding, from the one
            // place that ratio is written down.
            fuzzyCompare(grid.gap, grid.cellSize * GeometryTuning.spacingRatio, 0.001);
            grid.destroy();
        }
    }
}
