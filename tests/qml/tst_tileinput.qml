import QtQuick
import QtTest
import org.kde.frappedock

/*
 * Event delivery: that a click on a tile arrives at the C++ boundary with the
 * button and modifiers intact.
 *
 * What the combinations *mean* is test_dispatch's business, and whether the
 * compositor lets them through is tests/manual/03-bindings.md. This is only the
 * wire between the two, which is exactly the part that was silently missing
 * before — the tile used to emit an id and nothing else.
 */
TestCase {
    id: testCase
    name: "TileInput"
    when: windowShown
    visible: true
    width: 800
    height: 400

    // Snapshotted in initTestCase, not bound: a binding would track the very
    // changes these tests make and cleanup would restore nothing.
    property int originalTileSize: 0

    function initTestCase() {
        testCase.originalTileSize = FrappeConfig.tileSize;
    }

    function cleanup() {
        FrappeConfig.tileSize = testCase.originalTileSize;
    }

    Component {
        id: dockComponent
        Dock {}
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }

    /// An application tile and a separator, so the separator's exemption from
    /// input can be checked against a tile that is not exempt.
    function makeDock() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "alpha", name: "Alpha", iconName: "",
                                                          kind: 0, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "sep", name: "", iconName: "",
                                                          kind: 4, isRunning: false, windowCount: 0, isPinned: true },
                                                        { tileId: "beta", name: "Beta", iconName: "",
                                                          kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        verify(dock);
        return dock;
    }

    function spyOn(dock, signalName) {
        let spy = createTemporaryObject(signalSpyComponent, testCase,
                                        { target: dock, signalName: signalName });
        verify(spy.valid);
        return spy;
    }

    function tileAt(dock, row) {
        let tile = findChild(dock, "repeater").itemAt(row);
        verify(tile);
        return tile;
    }

    function test_clickCarriesButtonAndModifiers_data() {
        return [
            { tag: "plain",         button: Qt.LeftButton,   modifiers: Qt.NoModifier },
            { tag: "meta",          button: Qt.LeftButton,   modifiers: Qt.MetaModifier },
            { tag: "alt",           button: Qt.LeftButton,   modifiers: Qt.AltModifier },
            { tag: "meta-alt",      button: Qt.LeftButton,   modifiers: Qt.MetaModifier | Qt.AltModifier },
            { tag: "middle",        button: Qt.MiddleButton, modifiers: Qt.NoModifier },
            { tag: "right",         button: Qt.RightButton,  modifiers: Qt.NoModifier },
        ];
    }

    function test_clickCarriesButtonAndModifiers(data) {
        let dock = makeDock();
        let spy = spyOn(dock, "tileClicked");

        // The third tile, so an off-by-one between index and id would show up —
        // and it is past the separator, which occupies a cell of its own.
        mouseClick(tileAt(dock, 2), undefined, undefined, data.button, data.modifiers);

        compare(spy.count, 1);
        compare(spy.signalArguments[0][0], "beta");
        compare(spy.signalArguments[0][1], data.button);
        compare(spy.signalArguments[0][2], data.modifiers);
    }

    function test_pressAndHoldEmitsHeldAndNotClicked() {
        let dock = makeDock();
        let held = spyOn(dock, "tileHeld");
        let clicked = spyOn(dock, "tileClicked");

        let tile = tileAt(dock, 0);
        mousePress(tile);
        // Comfortably past MouseArea's press-and-hold threshold.
        wait(1000);
        mouseRelease(tile);

        compare(held.count, 1);
        compare(held.signalArguments[0][0], "alpha");
        // A hold that also fired a click would launch the application underneath
        // the popup it just opened.
        compare(clicked.count, 0);
    }

    function test_shortPressEmitsClickedAndNotHeld() {
        let dock = makeDock();
        let held = spyOn(dock, "tileHeld");
        let clicked = spyOn(dock, "tileClicked");

        mouseClick(tileAt(dock, 0));

        compare(clicked.count, 1);
        compare(held.count, 0);
    }

    function test_separatorSwallowsNothingAndEmitsNothing() {
        let dock = makeDock();
        let clicked = spyOn(dock, "tileClicked");
        let held = spyOn(dock, "tileHeld");

        let separator = tileAt(dock, 1);
        mouseClick(separator);
        mouseClick(separator, undefined, undefined, Qt.RightButton);

        // A separator has no application behind it, so there is nothing to send.
        compare(clicked.count, 0);
        compare(held.count, 0);
    }

    function test_eachTileReportsItsOwnId() {
        let dock = makeDock();
        let spy = spyOn(dock, "tileClicked");

        mouseClick(tileAt(dock, 0));
        mouseClick(tileAt(dock, 2));

        compare(spy.count, 2);
        compare(spy.signalArguments[0][0], "alpha");
        compare(spy.signalArguments[1][0], "beta");
    }
}
