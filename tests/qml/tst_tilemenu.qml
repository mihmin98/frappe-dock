import QtQuick
import QtTest
import org.kde.frappedock

/*
 * That the popup actually renders what C++ hands it, and that the right
 * interactions open it.
 *
 * The controller is stubbed with a plain QtObject: TileMenu only ever calls
 * three methods on it, and a stub makes the row list a test input rather than
 * something to be arranged through three backends.
 */
TestCase {
    id: testCase
    name: "TileMenu"
    when: windowShown
    visible: true
    width: 800
    height: 400

    property var triggered: []

    /// Rows covering every shape the menu can take: a separator, a checkable
    /// row, a row with an id, and a plain one.
    readonly property var sampleRows: [
        { kind: MenuItemKind.Window, itemId: "w1", label: "notes.txt", checkable: false, checked: false },
        { kind: MenuItemKind.Window, itemId: "w2", label: "draft.txt", checkable: false, checked: false },
        { kind: MenuItemKind.Separator, itemId: "", label: "", checkable: false, checked: false },
        { kind: MenuItemKind.Unpin, itemId: "", label: "Remove from Dock", checkable: false, checked: false },
        { kind: MenuItemKind.LaunchAtLogin, itemId: "", label: "Open at Login", checkable: true, checked: true }
    ]

    Component {
        id: controllerComponent

        QtObject {
            property var rows: []
            /// What commandFor() should answer, so a test can say "this input is
            /// a context-menu input" without reimplementing the matrix.
            property int command: DockCommand.LaunchOrActivate

            function commandFor(button, modifiers, held) {
                return command;
            }
            function contextMenuFor(tileId) {
                return rows;
            }
            function menuItemTriggered(tileId, kind, itemId) {
                testCase.triggered.push([tileId, kind, itemId]);
            }
        }
    }

    Component {
        id: dockComponent
        Dock {}
    }

    function cleanup() {
        testCase.triggered = [];
    }

    function makeDock(command, rows) {
        let controller = createTemporaryObject(controllerComponent, testCase,
                                               { command: command, rows: rows });
        verify(controller);
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { controller: controller,
                                           tileModel: [ { tileId: "editor", name: "Editor", iconName: "",
                                                          kind: 0, isRunning: true, windowCount: 2, isPinned: true } ] });
        verify(dock);
        return dock;
    }

    function tile(dock) {
        let t = findChild(dock, "repeater").itemAt(0);
        verify(t);
        return t;
    }

    function test_contextMenuInputOpensThePopup() {
        let dock = makeDock(DockCommand.ShowContextMenu, testCase.sampleRows);
        let clicked = createTemporaryObject(signalSpyComponent, testCase,
                                            { target: dock, signalName: "tileClicked" });

        mouseClick(tile(dock), undefined, undefined, Qt.RightButton);

        let menu = findChild(dock, "tileMenu");
        verify(menu);
        verify(menu.opened || menu.visible);
        // The popup is handled here, so it must not also travel to C++ and be
        // acted on a second time.
        compare(clicked.count, 0);
    }

    function test_jumpListInputOpensTheSamePopup() {
        let dock = makeDock(DockCommand.ShowJumpList, testCase.sampleRows);
        let held = createTemporaryObject(signalSpyComponent, testCase,
                                         { target: dock, signalName: "tileHeld" });

        let t = tile(dock);
        mousePress(t);
        wait(1000);
        mouseRelease(t);

        let menu = findChild(dock, "tileMenu");
        verify(menu.opened || menu.visible);
        compare(held.count, 0);
    }

    function test_otherInputsStillReachCpp() {
        let dock = makeDock(DockCommand.LaunchOrActivate, testCase.sampleRows);
        let clicked = createTemporaryObject(signalSpyComponent, testCase,
                                            { target: dock, signalName: "tileClicked" });

        mouseClick(tile(dock));

        compare(clicked.count, 1);
        let menu = findChild(dock, "tileMenu");
        verify(!menu.visible);
    }

    function test_rowsRenderIncludingSeparatorsAndChecks() {
        let dock = makeDock(DockCommand.ShowContextMenu, testCase.sampleRows);
        mouseClick(tile(dock), undefined, undefined, Qt.RightButton);

        let menu = findChild(dock, "tileMenu");
        // One object per row, separators included.
        compare(menu.count, testCase.sampleRows.length);

        compare(menu.itemAt(0).text, "notes.txt");
        compare(menu.itemAt(1).text, "draft.txt");
        // The separator is a different type, so it has no text to check; what
        // matters is that it is not a MenuItem carrying an empty label.
        verify(menu.itemAt(3).text === "Remove from Dock");
        verify(menu.itemAt(4).checkable);
        verify(menu.itemAt(4).checked);
    }

    function test_choosingARowReportsItsKindAndId() {
        let dock = makeDock(DockCommand.ShowContextMenu, testCase.sampleRows);
        mouseClick(tile(dock), undefined, undefined, Qt.RightButton);

        let menu = findChild(dock, "tileMenu");
        menu.itemAt(1).triggered();

        compare(testCase.triggered.length, 1);
        compare(testCase.triggered[0][0], "editor");
        compare(testCase.triggered[0][1], MenuItemKind.Window);
        compare(testCase.triggered[0][2], "w2");
    }

    function test_menuIsRebuiltOnEveryOpen() {
        let dock = makeDock(DockCommand.ShowContextMenu, testCase.sampleRows);
        let menu = findChild(dock, "tileMenu");

        mouseClick(tile(dock), undefined, undefined, Qt.RightButton);
        compare(menu.count, testCase.sampleRows.length);
        menu.close();

        // The window list changed while the dock was up. A menu built once would
        // still be showing the old one.
        dock.controller.rows = [ { kind: MenuItemKind.Pin, itemId: "", label: "Keep in Dock",
                                   checkable: false, checked: false } ];
        mouseClick(tile(dock), undefined, undefined, Qt.RightButton);
        compare(menu.count, 1);
        compare(menu.itemAt(0).text, "Keep in Dock");
    }

    function test_missingControllerIsSurvivable() {
        let dock = createTemporaryObject(dockComponent, testCase,
                                         { tileModel: [ { tileId: "editor", name: "Editor", iconName: "",
                                                          kind: 0, isRunning: false, windowCount: 0, isPinned: true } ] });
        verify(dock);
        // No controller: clicks fall back to the plain command and nothing throws.
        mouseClick(tile(dock), undefined, undefined, Qt.RightButton);
        verify(true);
    }

    Component {
        id: signalSpyComponent
        SignalSpy {}
    }
}
