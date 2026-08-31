import QtQuick
import QtTest
import org.kde.frappedock

/*
 * The running-application dot: when it shows, and where it sits relative to the
 * artwork for each dock edge.
 *
 * Everything is asserted against the artwork's box rather than the cell's, and
 * as a ratio of the tile size rather than in points, because an icon theme may
 * inset artwork within the cell and the dot must follow the art.
 */
TestCase {
    id: testCase
    name: "Indicator"
    when: windowShown
    visible: true
    width: 800
    height: 400

    // Snapshotted in initTestCase, not bound: a binding would track the very
    // changes these tests make and cleanup would restore nothing.
    property int originalTileSize: 0
    property int originalPosition: 0
    property bool originalShowIndicators: true

    function initTestCase() {
        testCase.originalTileSize = FrappeConfig.tileSize;
        testCase.originalPosition = FrappeConfig.position;
        testCase.originalShowIndicators = FrappeConfig.showRunningIndicators;
    }

    Component {
        id: dockComponent
        Dock {}
    }

    function cleanup() {
        FrappeConfig.tileSize = testCase.originalTileSize;
        FrappeConfig.position = testCase.originalPosition;
        FrappeConfig.showRunningIndicators = testCase.originalShowIndicators;
    }

    /// One running tile and one idle one, so every assertion below can tell the
    /// two states apart within a single dock.
    function makeDock() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "running", name: "Running", iconName: "",
                                                          kind: 0, isRunning: true, windowCount: 1, isPinned: true },
                                                        { tileId: "idle", name: "Idle", iconName: "",
                                                          kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        verify(dock);
        return dock;
    }

    function tileAt(dock, row) {
        let repeater = findChild(dock, "repeater");
        verify(repeater);
        let tile = repeater.itemAt(row);
        verify(tile);
        return tile;
    }

    function test_visibleIfRunning() {
        FrappeConfig.showRunningIndicators = true;
        let dock = makeDock();

        verify(findChild(tileAt(dock, 0), "indicator").visible);
        // Same dock, same config: the only difference is the tile's own state.
        verify(!findChild(tileAt(dock, 1), "indicator").visible);
    }

    function test_hiddenWhenDisabled() {
        FrappeConfig.showRunningIndicators = false;
        let dock = makeDock();

        verify(!findChild(tileAt(dock, 0), "indicator").visible);

        // And it is a live binding, not a construction-time read: the settings
        // page has to take effect without a restart.
        FrappeConfig.showRunningIndicators = true;
        verify(findChild(tileAt(dock, 0), "indicator").visible);
    }

    function test_correctPositionForBottomDock() {
        FrappeConfig.position = ConfigFacade.Bottom;
        let dock = makeDock();
        let tile = tileAt(dock, 0);
        let art = findChild(tile, "art");
        let indicator = findChild(tile, "indicator");

        // Outer edge: below the artwork, towards the screen border.
        fuzzyCompare(indicator.y + indicator.height / 2,
                     art.y + art.height + 0.21 * dock.iconSize, 0.01);
        fuzzyCompare(indicator.x + indicator.width / 2,
                     art.x + art.width / 2, 0.01);
    }

    function test_correctPositionForSideDock() {
        FrappeConfig.position = ConfigFacade.Left;
        let dock = makeDock();
        let tile = tileAt(dock, 0);
        let art = findChild(tile, "art");
        let indicator = findChild(tile, "indicator");

        // Inner edge: for a dock on the left, that is the artwork's right side,
        // facing the desktop rather than the screen border.
        fuzzyCompare(indicator.x + indicator.width / 2,
                     art.x + art.width + 0.21 * dock.iconSize, 0.01);
        fuzzyCompare(indicator.y + indicator.height / 2,
                     art.y + art.height / 2, 0.01);

        // Mirrored for a dock on the right.
        FrappeConfig.position = ConfigFacade.Right;
        let rightDock = makeDock();
        let rightTile = tileAt(rightDock, 0);
        let rightArt = findChild(rightTile, "art");
        let rightIndicator = findChild(rightTile, "indicator");

        fuzzyCompare(rightIndicator.x + rightIndicator.width / 2,
                     rightArt.x - 0.21 * rightDock.iconSize, 0.01);
        fuzzyCompare(rightIndicator.y + rightIndicator.height / 2,
                     rightArt.y + rightArt.height / 2, 0.01);
    }

    function test_dotScalesWithTileSize() {
        FrappeConfig.tileSize = 32;
        let small = findChild(tileAt(makeDock(), 0), "indicator");
        let smallDot = small.height;

        FrappeConfig.tileSize = 96;
        let large = findChild(tileAt(makeDock(), 0), "indicator");

        // 0.085 S, and a dock at one size is a uniform scale of a dock at another.
        fuzzyCompare(smallDot, 0.085 * 32, 0.01);
        fuzzyCompare(large.height, 0.085 * 96, 0.01);
        fuzzyCompare(large.height / smallDot, 3.0, 0.01);
    }

    function test_singleDotUnlessCountingIsEnabled() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "many", name: "Many", iconName: "",
                                                          kind: 0, isRunning: true, windowCount: 5, isPinned: true } ] });
        verify(dock);
        let indicator = findChild(tileAt(dock, 0), "indicator");

        // Counting is off by default, so five windows still read as "running".
        compare(indicator.dotCount, 1);

        indicator.showWindowCount = true;
        // Capped: past a few, dots stop being countable.
        compare(indicator.dotCount, 3);
    }

    function test_separatorHasNoIndicator() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "sep", name: "", iconName: "",
                                                          kind: 4, isRunning: true, windowCount: 1, isPinned: true } ] });
        verify(dock);
        // Nothing is running behind a separator, whatever the model says.
        verify(!findChild(tileAt(dock, 0), "indicator").visible);
    }
}
