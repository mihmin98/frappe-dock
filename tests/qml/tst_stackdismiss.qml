import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * Dismissing an open stack.
 *
 * Regression — manual checklist 05, run 1, finding 1. A stack could only be
 * closed by clicking its own folder tile again or by activating an entry:
 * clicking anywhere else left it open. Two things were wrong at once, and
 * either would have been enough on its own — `closeRequested` was declared and
 * handled but emitted by nothing, and the surface's input region covered only
 * the shelf and the stack, so a click beside them never arrived to be noticed.
 *
 * The region half is what the last test here is about. It cannot be checked by
 * clicking, because an offscreen test has no input region at all — every click
 * lands wherever the scene says it should. What it can check is the rectangle
 * the dock publishes for the platform to apply, which is the same number.
 */
TestCase {
    id: testCase

    name: "StackDismiss"
    when: windowShown
    visible: true
    width: 900
    height: 600

    readonly property string folder: "/stack/Dismiss"

    property int originalSpeed: 0

    function initTestCase() {
        testCase.originalSpeed = FrappeConfig.animationSpeed;
        // A stack that is still animating open is a stack whose bounds are not
        // yet what they will be, and every coordinate below would be a race.
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
        ListModel {}
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }

    function makeDock() {
        let tiles = createTemporaryObject(modelComponent, testCase);
        verify(tiles);
        tiles.append({ tileId: "alpha", name: "Alpha", iconName: "", kind: TileKind.Application,
                       isRunning: false, windowCount: 0, isPinned: true });
        tiles.append({ tileId: testCase.folder, name: "Dismiss", iconName: "",
                       kind: TileKind.Folder,
                       isRunning: false, windowCount: 0, isPinned: true });

        let stackModel = StackFixture.create(testCase.folder);
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: tiles, stackModel: stackModel,
                                           width: testCase.width, height: testCase.height });
        verify(dock);

        StackFixture.addFiles(stackModel, 6);
        StackFixture.setStatus(stackModel, 2); // Ready
        StackFixture.notifyChange(stackModel);
        return dock;
    }

    /// Opens the folder tile's stack directly. Going through a synthesised click
    /// on the tile would be testing the tile's dispatch, which tst_tileinput
    /// already owns; what is under test here is only how the stack closes.
    function openStack(dock) {
        dock.toggleStack(testCase.folder, 1);
        waitForRendering(dock);
        compare(dock.openStackPath, testCase.folder);
    }

    function stackOf(dock) {
        let stack = findChild(dock, "stackPopup");
        verify(stack, "found the stack");
        return stack;
    }

    function test_clickingAwayFromTheStackClosesIt() {
        let dock = makeDock();
        openStack(dock);

        // The far corner from a bottom dock: not the shelf, not the stack.
        mouseClick(dock, 8, 8);
        compare(dock.openStackPath, "");
    }

    /*
     * Missing an icon is not the same as clicking away.
     *
     * The corner of the stack is its own padding — no entry, no header control.
     * A click there has to do nothing at all: if it falls through to the
     * backdrop, missing an icon by two pixels closes the stack, which is the
     * worst possible reading of a near miss.
     *
     * Not the stack's centre, which is an entry: clicking an entry opens it and
     * closes the stack, and that is correct.
     */
    function test_clickingTheStacksOwnPaddingDoesNotCloseIt() {
        let dock = makeDock();
        openStack(dock);
        let stack = stackOf(dock);

        mouseClick(dock, stack.x + 2, stack.y + 2);
        compare(dock.openStackPath, testCase.folder);

        mouseClick(dock, stack.x + stack.width - 2, stack.y + stack.height - 2);
        compare(dock.openStackPath, testCase.folder);
    }

    function test_escapeClosesTheStack() {
        let dock = makeDock();
        openStack(dock);

        keyClick(Qt.Key_Escape);
        compare(dock.openStackPath, "");
    }

    function test_escapeWithNoStackOpenDoesNothing() {
        let dock = makeDock();
        compare(dock.openStackPath, "");

        // Nothing to close, and nothing to swallow the key from whatever else
        // might want it.
        keyClick(Qt.Key_Escape);
        compare(dock.openStackPath, "");
    }

    function test_clickingAnotherTileIsNotSwallowedByTheBackdrop() {
        let dock = makeDock();
        openStack(dock);

        // Clicking the folder tile again is the other way to dismiss, and it has
        // to keep working: a backdrop drawn over the shelf would eat it and the
        // toggle would never see it.
        dock.toggleStack(testCase.folder, 1);
        compare(dock.openStackPath, "");
    }

    /*
     * The input region, which is the half of the defect a click cannot reach.
     *
     * While a stack is open the dock has to take clicks over the whole output,
     * or a click beside the stack goes to whatever is behind the dock and the
     * stack stays open. While none is open it must take none of them back, or
     * the dock swallows the desktop.
     */
    function test_theInputRegionCoversTheOutputWhileAStackIsOpen() {
        let dock = makeDock();
        let spy = createTemporaryObject(signalSpyComponent, testCase,
                                        { target: dock, signalName: "stackRegionChanged" });
        verify(spy);

        openStack(dock);
        verify(spy.count > 0, "the dock published a region");

        let region = spy.signalArguments[spy.count - 1][0];
        compare(region.x, 0);
        compare(region.y, 0);
        compare(region.width, dock.width);
        compare(region.height, dock.height);

        spy.clear();
        dock.closeStack();
        waitForRendering(dock);
        verify(spy.count > 0, "the dock published the region going away");

        let closed = spy.signalArguments[spy.count - 1][0];
        compare(closed.width, 0);
        compare(closed.height, 0);
    }
}
