import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * Spring-loading, as wired up in the view.
 *
 * The timing rules are test_springload's business. What is checked here is the
 * wiring the C++ tests cannot see: that a drag resting on a tile reaches the
 * loader at all, that leaving the tile and dropping on it both call it off, and
 * that firing activates the tile the pointer was over.
 */
TestCase {
    id: testCase
    name: "SpringLoad"
    when: windowShown
    visible: true
    width: 800
    height: 400

    readonly property url fileUrl: "file:///home/someone/notes.txt"
    /// Short, so the suite does not wait on a human-scale delay.
    readonly property int delay: 120

    property int originalSpeed: 0
    property int originalDelay: 0

    function initTestCase() {
        testCase.originalSpeed = FrappeConfig.animationSpeed;
        testCase.originalDelay = FrappeConfig.springLoadDelay;
        FrappeConfig.animationSpeed = 0;
        FrappeConfig.springLoadDelay = testCase.delay;
    }

    function cleanupTestCase() {
        FrappeConfig.animationSpeed = testCase.originalSpeed;
        FrappeConfig.springLoadDelay = testCase.originalDelay;
    }

    Component {
        id: dockComponent
        Dock {}
    }

    Component {
        id: modelComponent
        ListModel {}
    }

    /// Records what it was asked to activate, and accepts nothing else: a
    /// spring-load must not be a disguised drop.
    Component {
        id: controllerComponent
        QtObject {
            property var activated: []
            /// Whether the tiles will take the dragged files. Set false to make
            /// every tile refuse, as one that does not declare the type does.
            property bool dropAccepted: true

            function activateTile(tileId) {
                activated = activated.concat([tileId]);
            }

            function evaluateRegionDrop(region, urls) {
                return { accepted: false, rejection: RegionDropRejection.WrongRegion,
                         itemKind: DropItemKind.File, expectedRegion: TileRegion.Files,
                         detail: "" };
            }

            function acceptRegionDrop(region, urls) {
                return false;
            }

            function evaluateDrop(tileId, urls) {
                return { accepted: dropAccepted,
                         rejection: dropAccepted ? DropRejection.None
                                                 : DropRejection.UnsupportedType,
                         detail: "Debian package", appName: tileId };
            }

            function openDroppedFiles(tileId, urls) {
                return true;
            }
        }
    }

    function makeDock() {
        let model = createTemporaryObject(modelComponent, testCase);
        verify(model);

        let names = ["alpha", "beta", "gamma"];
        for (let i = 0; i < names.length; ++i) {
            model.append({ tileId: names[i], name: names[i], iconName: "", kind: 0,
                           isRunning: false, windowCount: 0, isPinned: true });
        }

        let controller = createTemporaryObject(controllerComponent, testCase);
        verify(controller);

        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: model, controller: controller });
        verify(dock);
        return dock;
    }

    function strip(dock) {
        let item = findChild(dock, "stripDrop");
        verify(item);
        return item;
    }

    function tileAt(dock, row) {
        let tile = findChild(dock, "repeater").itemAt(row);
        verify(tile);
        return tile;
    }

    function centreOf(dock, row) {
        let tile = tileAt(dock, row);
        return tile.mapToItem(strip(dock), tile.width / 2, tile.height / 2);
    }

    function test_restingOnATileActivatesIt() {
        let dock = makeDock();
        let at = centreOf(dock, 1);

        DragSimulator.enter(strip(dock), at.x, at.y, [testCase.fileUrl]);
        // Armed, and saying so: the countdown is visible before it fires.
        compare(tileAt(dock, 1).springArmed, true);
        verify(findChild(tileAt(dock, 1), "springIndicator").visible);
        compare(dock.controller.activated.length, 0);

        tryCompare(dock.controller, "activated", ["beta"], 10 * testCase.delay);
        compare(tileAt(dock, 1).springArmed, false);

        DragSimulator.leave(strip(dock));
    }

    /// Dragging past a tile must not open it — the case that makes an
    /// accidental spring-load expensive.
    function test_passingOverATileDoesNotActivateIt() {
        let dock = makeDock();
        let first = centreOf(dock, 0);
        let last = centreOf(dock, 2);

        DragSimulator.enter(strip(dock), first.x, first.y, [testCase.fileUrl]);
        compare(tileAt(dock, 0).springArmed, true);

        // Moving on re-aims the countdown rather than leaving it on the tile
        // the drag has already left.
        DragSimulator.move(strip(dock), last.x, last.y);
        compare(tileAt(dock, 0).springArmed, false);
        compare(tileAt(dock, 2).springArmed, true);

        DragSimulator.leave(strip(dock));
        wait(3 * testCase.delay);
        compare(dock.controller.activated.length, 0);
    }

    /// Regression: a tile that has refused the drag used to spring open anyway,
    /// because the hover was reported whatever the verdict. Reading the refusal
    /// message takes longer than the delay, so the application launched while
    /// the user was still working out why the drop would not be taken — which
    /// read as the refusal being ignored. One refusal, one meaning.
    function test_refusingTileDoesNotSpringOpen() {
        let dock = makeDock();
        dock.controller.dropAccepted = false;
        let at = centreOf(dock, 1);

        DragSimulator.enter(strip(dock), at.x, at.y, [testCase.fileUrl]);

        // The refusal is still shown — suppressing the spring must not also
        // suppress the explanation.
        compare(tileAt(dock, 1).dropState, "rejected");
        compare(tileAt(dock, 1).springArmed, false);

        wait(3 * testCase.delay);
        compare(dock.controller.activated.length, 0);

        DragSimulator.leave(strip(dock));
    }

    /// The suppression is the verdict's doing, not a broken countdown: the same
    /// tile springs open as soon as it would take the drop.
    function test_acceptingTileStillSpringsOpenAfterARefusal() {
        let dock = makeDock();
        dock.controller.dropAccepted = false;
        let at = centreOf(dock, 1);

        DragSimulator.enter(strip(dock), at.x, at.y, [testCase.fileUrl]);
        compare(tileAt(dock, 1).springArmed, false);
        DragSimulator.leave(strip(dock));

        dock.controller.dropAccepted = true;
        DragSimulator.enter(strip(dock), at.x, at.y, [testCase.fileUrl]);
        compare(tileAt(dock, 1).springArmed, true);
        tryCompare(dock.controller, "activated", ["beta"], 10 * testCase.delay);

        DragSimulator.leave(strip(dock));
    }

    /// A drop is the user saying what they wanted; the tile must not also
    /// spring open behind it.
    function test_droppingCancelsTheCountdown() {
        let dock = makeDock();
        let at = centreOf(dock, 0);

        DragSimulator.enter(strip(dock), at.x, at.y, [testCase.fileUrl]);
        DragSimulator.drop(strip(dock), at.x, at.y);

        wait(3 * testCase.delay);
        compare(dock.controller.activated.length, 0);
    }

    /// Off means off, whatever the pointer does.
    function test_zeroDelayNeverSpringLoads() {
        FrappeConfig.springLoadDelay = 0;

        let dock = makeDock();
        let at = centreOf(dock, 0);

        DragSimulator.enter(strip(dock), at.x, at.y, [testCase.fileUrl]);
        compare(tileAt(dock, 0).springArmed, false);
        wait(3 * testCase.delay);
        compare(dock.controller.activated.length, 0);

        DragSimulator.leave(strip(dock));
        FrappeConfig.springLoadDelay = testCase.delay;
    }
}
