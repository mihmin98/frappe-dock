import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * The stack List view: one row per entry, and the same navigation the grid has.
 */
Item {
    id: root

    width: 600
    height: 500

    Component {
        id: listComponent

        StackList {
            cellSize: 48
            visibleRows: 4
        }
    }

    TestCase {
        id: testCase

        name: "StackList"
        when: windowShown

        function makeList(rootPath) {
            let model = StackFixture.create(rootPath);
            let list = listComponent.createObject(root, { model: model });
            verify(list, "list created");
            return list;
        }

        function ready(view) {
            StackFixture.setStatus(view.model, 2); // Ready
            StackFixture.notifyChange(view.model);
        }

        function listView(item) {
            for (let i = 0; i < item.children.length; ++i) {
                if (item.children[i] instanceof ListView) {
                    return item.children[i];
                }
            }
            return null;
        }

        function test_rendersOneRowPerEntry() {
            let list = makeList("/stack/Root");
            StackFixture.addDir(list.model, "alpha");
            StackFixture.addFile(list.model, "notes.txt");
            ready(list);

            compare(listView(list).count, 2);

            StackFixture.removeEntry(list.model, "notes.txt");
            StackFixture.notifyChange(list.model);
            compare(listView(list).count, 1);
            list.destroy();
        }

        function test_rowsAreOneCellTall() {
            let list = makeList("/stack/Root");
            StackFixture.addFiles(list.model, 3);
            ready(list);

            let view = listView(list);
            waitForRendering(view);
            // One cell plus the gap either side, the same rule the dock's shelf
            // thickness follows.
            fuzzyCompare(list.rowHeight, list.cellSize + list.gap, 0.001);
            fuzzyCompare(view.contentHeight, 3 * list.rowHeight, 0.01);
            list.destroy();
        }

        function test_scrollsPastTheVisibleRows() {
            let list = makeList("/stack/Root");
            StackFixture.addFiles(list.model, 30);
            ready(list);

            let view = listView(list);
            waitForRendering(view);
            verify(view.contentHeight > view.height, "content taller than the viewport");

            view.positionViewAtEnd();
            verify(view.contentY > 0, "view scrolled to the end");
            list.destroy();
        }

        function test_doesNotWidenWithItsContents() {
            let list = makeList("/stack/Root");
            StackFixture.addFile(list.model, "a.txt");
            ready(list);
            let narrow = list.implicitWidth;

            StackFixture.addFile(list.model, "a-name-very-much-longer-than-the-others.txt");
            StackFixture.notifyChange(list.model);

            // A list that grew to fit its longest file name would change width
            // every time the folder's contents changed.
            compare(list.implicitWidth, narrow);
            list.destroy();
        }

        function test_drillDownAndReturn() {
            let list = makeList("/stack/Root");
            StackFixture.addDir(list.model, "inner");
            StackFixture.addFile(list.model, "outer.txt");
            ready(list);

            verify(list.model.enterFolder(0), "row 0 is the folder");
            compare(list.model.path, "/stack/Root/inner");
            StackFixture.addFile(list.model, "deep.txt");
            ready(list);
            compare(listView(list).count, 1);

            verify(list.model.goUp(), "back up");
            compare(list.model.path, "/stack/Root");
            list.destroy();
        }

        function test_statesAreDistinct() {
            let list = makeList("/stack/Root");
            ready(list);
            compare(list.empty, true);

            StackFixture.setStatus(list.model, 1); // Loading
            StackFixture.notifyChange(list.model);
            compare(list.loading, true);
            compare(listView(list).count, 0);

            StackFixture.setStatus(list.model, 3); // Failed
            StackFixture.notifyChange(list.model);
            compare(list.failed, true);
            compare(list.empty, false);
            list.destroy();
        }

        function test_proportionsFollowCellSize() {
            let list = makeList("/stack/Root");
            StackFixture.addFiles(list.model, 4);
            ready(list);

            let small = { w: list.implicitWidth, h: list.implicitHeight, row: list.rowHeight };
            list.cellSize = 96;

            fuzzyCompare(list.rowHeight, small.row * 2, small.row * 0.01);
            fuzzyCompare(list.implicitWidth, small.w * 2, small.w * 0.01);
            fuzzyCompare(list.implicitHeight, small.h * 2, small.h * 0.01);
            list.destroy();
        }
    }
}
