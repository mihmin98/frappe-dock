import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * The open stack: which arrangement it shows, and where it sits.
 *
 * The placement arithmetic itself is covered headless in
 * tests/unit/test_stackpositioning.cpp. What this adds is that the QML actually
 * binds to it — that the anchor reaching StackAnchor is the resting one, and
 * that switching the stored mode swaps the view.
 */
Item {
    id: root

    width: 1200
    height: 900

    Component {
        id: popupComponent

        StackPopup {
            cellSize: 48
            outputWidth: 1200
            outputHeight: 900
            anchorCentre: 600
        }
    }

    TestCase {
        id: testCase

        name: "StackPopup"
        when: windowShown

        function makePopup(folder) {
            let model = StackFixture.create(folder);
            let popup = popupComponent.createObject(root, { model: model, folderPath: folder, open: true });
            verify(popup, "popup created");
            StackFixture.addFiles(model, 6);
            StackFixture.setStatus(model, 2);
            StackFixture.notifyChange(model);
            return popup;
        }

        function contentOf(popup) {
            for (let i = 0; i < popup.children.length; ++i) {
                if (popup.children[i] instanceof Loader) {
                    return popup.children[i].item;
                }
            }
            return null;
        }

        function init() {
            // Each test starts from the shipped defaults rather than from
            // whatever the previous one wrote: the settings are process-wide.
            StackSettings.setDefaultSortOrder(StackSortOrder.Name);
        }

        function test_defaultsToTheGrid() {
            let popup = makePopup("/stack/Defaults");
            compare(popup.currentMode, StackViewMode.Grid);
            verify(contentOf(popup) instanceof StackGrid, "grid is showing");
            popup.destroy();
        }

        function test_followsTheStoredViewMode() {
            let popup = makePopup("/stack/Modes");

            StackSettings.setViewMode("/stack/Modes", StackViewMode.List);
            compare(popup.currentMode, StackViewMode.List);
            verify(contentOf(popup) instanceof StackList, "list is showing");

            StackSettings.setViewMode("/stack/Modes", StackViewMode.Fan);
            compare(popup.currentMode, StackViewMode.Fan);
            verify(contentOf(popup) instanceof StackFan, "fan is showing");

            StackSettings.setViewMode("/stack/Modes", StackViewMode.Grid);
            verify(contentOf(popup) instanceof StackGrid, "back to the grid");
            popup.destroy();
        }

        function test_appliesTheStoredSortOrderToTheModel() {
            let popup = makePopup("/stack/Sorting");
            compare(popup.model.sortOrder, StackSortOrder.Name);

            StackSettings.setSortOrder("/stack/Sorting", StackSortOrder.Kind);
            compare(popup.model.sortOrder, StackSortOrder.Kind);
            popup.destroy();
        }

        function test_followsTheDefaultSortWhenItHasNoPreference() {
            let popup = makePopup("/stack/NoPreference");
            compare(popup.model.sortOrder, StackSortOrder.Name);

            // The default moving affects every folder without an opinion, and
            // the signal that carries it names no folder at all.
            StackSettings.setDefaultSortOrder(StackSortOrder.DateModified);
            compare(popup.model.sortOrder, StackSortOrder.DateModified);
            popup.destroy();
        }

        function test_sitsClearOfTheShelfForABottomDock() {
            let popup = makePopup("/stack/Bottom");
            popup.dockPosition = ConfigFacade.Bottom;
            waitForRendering(popup);

            let gap = popup.cellSize * GeometryTuning.spacingRatio;
            let shelfInner = gap / 3 + popup.cellSize + 2 * gap;
            // The stack's bottom edge stands off the shelf by one gap.
            fuzzyCompare(popup.outputHeight - (popup.y + popup.height), shelfInner + gap, 0.001);
            popup.destroy();
        }

        function test_sitsClearOfTheShelfForASideDock() {
            let popup = makePopup("/stack/Side");
            popup.dockPosition = ConfigFacade.Left;
            waitForRendering(popup);

            let gap = popup.cellSize * GeometryTuning.spacingRatio;
            let shelfInner = gap / 3 + popup.cellSize + 2 * gap;
            fuzzyCompare(popup.x, shelfInner + gap, 0.001);

            popup.dockPosition = ConfigFacade.Right;
            waitForRendering(popup);
            fuzzyCompare(popup.outputWidth - (popup.x + popup.width), shelfInner + gap, 0.001);
            popup.destroy();
        }

        /// The §11 regression, at the level the QML owns: the position binding
        /// must depend on the anchor and nothing that pointer motion touches.
        function test_positionDoesNotMoveWhileTheAnchorHolds() {
            let popup = makePopup("/stack/NoJump");
            waitForRendering(popup);
            let x = popup.x;
            let y = popup.y;

            // Whatever else changes in the scene, the stack holds still. The
            // anchor is the resting centre, so pointer motion — which moves only
            // the drawn tile geometry — has nothing here to move.
            for (let i = 0; i < 10; ++i) {
                StackFixture.notifyChange(popup.model);
                wait(1);
                compare(popup.x, x);
                compare(popup.y, y);
            }
            popup.destroy();
        }

        function test_movingTheAnchorMovesTheStack() {
            let popup = makePopup("/stack/Follow");
            waitForRendering(popup);
            let before = popup.x;

            // A reflow moves the tile, and the stack has to follow it — the
            // binding must be live, not a value copied when it opened.
            popup.anchorCentre = 300;
            compare(popup.x, before - 300);
            popup.destroy();
        }

        function test_closingReturnsToTheRootFolder() {
            let popup = makePopup("/stack/Reset");
            StackFixture.addDir(popup.model, "aaa-inner");
            StackFixture.notifyChange(popup.model);

            verify(popup.model.enterFolder(0), "row 0 is the folder");
            compare(popup.model.path, "/stack/Reset/aaa-inner");

            popup.open = false;
            // Reopening three folders deep into wherever it was left is not what
            // clicking a folder tile should do.
            compare(popup.model.path, "/stack/Reset");
            popup.destroy();
        }
    }
}
