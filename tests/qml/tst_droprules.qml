import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * Drop feedback, as it appears on screen.
 *
 * Which drops the rules accept is test_droprules' business. What is checked
 * here is the part that only exists while a drag is in the air: that a target
 * which will take the drop looks different from one that will refuse it, and
 * that a refusal says why in words rather than declining in silence.
 *
 * The drags are synthesised — DragSimulator posts the same drag events the
 * platform would — because an external drag is not a mouse gesture and QML has
 * no way to start one.
 */
TestCase {
    id: testCase
    name: "DropRules"
    when: windowShown
    visible: true
    width: 800
    height: 400

    readonly property url appUrl: "file:///usr/share/applications/alpha.desktop"
    readonly property url fileUrl: "file:///home/someone/notes.txt"

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
        ListModel {}
    }

    /*
     * A controller that answers the way the real rules do, without the disk.
     *
     * The verdicts are the ones test_droprules pins down in C++; repeating the
     * classification here would be testing the fake, so it answers from the
     * URL suffix and nothing more.
     */
    Component {
        id: controllerComponent
        QtObject {
            property var dropped: []

            function isApp(urls) {
                return urls.length > 0 && String(urls[0]).endsWith(".desktop");
            }

            function kindOf(urls) {
                return isApp(urls) ? DropItemKind.Application : DropItemKind.File;
            }

            function evaluateRegionDrop(region, urls) {
                if (urls.length === 0) {
                    return { accepted: false, rejection: RegionDropRejection.UnknownItem,
                             itemKind: DropItemKind.Unknown, expectedRegion: region, detail: "" };
                }

                let wantsPinned = isApp(urls);
                let isPinnedRegion = region === TileRegion.Pinned;
                if (wantsPinned === isPinnedRegion) {
                    return { accepted: true, rejection: RegionDropRejection.None,
                             itemKind: kindOf(urls), expectedRegion: region, detail: "" };
                }

                return { accepted: false, rejection: RegionDropRejection.WrongRegion,
                         itemKind: kindOf(urls),
                         expectedRegion: wantsPinned ? TileRegion.Pinned : TileRegion.Files,
                         detail: "" };
            }

            /// Tile-level drops: this fake's applications open files and
            /// nothing else, which is enough to tell the two highlights apart.
            function evaluateDrop(tileId, urls) {
                let ok = !isApp(urls) && tileId !== "gamma";
                return { accepted: ok,
                         rejection: ok ? DropRejection.None : DropRejection.UnsupportedType,
                         detail: "plain text document", appName: tileId };
            }

            function openDroppedFiles(tileId, urls) {
                if (!evaluateDrop(tileId, urls).accepted) {
                    return false;
                }
                dropped = dropped.concat([String(urls[0])]);
                return true;
            }

            function acceptRegionDrop(region, urls) {
                if (!evaluateRegionDrop(region, urls).accepted) {
                    return false;
                }
                dropped = dropped.concat([String(urls[0])]);
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

    function stripDrop(dock) {
        let item = findChild(dock, "stripDrop");
        verify(item);
        return item;
    }

    function refusalText(dock) {
        let item = findChild(dock, "dropRefusalText");
        verify(item);
        return item;
    }

    function refusalLabel(dock) {
        let item = findChild(dock, "dropRefusalLabel");
        verify(item);
        return item;
    }

    /// A point on the shelf beside the tiles — in the inner padding, where the
    /// strip itself is the target rather than any cell.
    function stripPoint(dock) {
        let strip = stripDrop(dock);
        return Qt.point(dock.gap / 2, strip.height / 2);
    }

    function tileAt(dock, row) {
        let tile = findChild(dock, "repeater").itemAt(row);
        verify(tile);
        return tile;
    }

    function centreOf(dock, row) {
        let tile = tileAt(dock, row);
        return tile.mapToItem(stripDrop(dock), tile.width / 2, tile.height / 2);
    }

    function test_acceptedDropHighlightsTheStripAndSaysNothing() {
        let dock = makeDock();
        let strip = stripDrop(dock);
        compare(strip.dropState, "none");

        let at = stripPoint(dock);
        DragSimulator.enter(strip, at.x, at.y, [testCase.appUrl]);

        compare(strip.dropState, "accepted");
        verify(!refusalLabel(dock).visible);

        DragSimulator.leave(strip);
    }

    function test_rejectedDropMarksTheStripAndExplainsWhy() {
        let dock = makeDock();
        let strip = stripDrop(dock);

        let at = stripPoint(dock);
        DragSimulator.enter(strip, at.x, at.y, [testCase.fileUrl]);

        compare(strip.dropState, "rejected");
        verify(refusalLabel(dock).visible);
        // The wording is the dock's, not the test's; what matters is that it
        // names both what was dropped and where it does not belong.
        verify(refusalText(dock).text.length > 0);
        compare(refusalText(dock).text, dock.dropRefusal);

        DragSimulator.leave(strip);
    }

    /// Leaving takes the message with it: a refusal that outlives the drag is
    /// a complaint about nothing.
    function test_leavingClearsTheFeedback() {
        let dock = makeDock();
        let strip = stripDrop(dock);
        let at = stripPoint(dock);

        DragSimulator.enter(strip, at.x, at.y, [testCase.fileUrl]);
        verify(refusalLabel(dock).visible);

        DragSimulator.leave(strip);
        compare(strip.dropState, "none");
        verify(!refusalLabel(dock).visible);
        compare(dock.dropRefusal, "");
    }

    function test_droppingAnApplicationPinsItAndDroppingAFileDoesNot() {
        let dock = makeDock();
        let strip = stripDrop(dock);
        let at = stripPoint(dock);

        DragSimulator.enter(strip, at.x, at.y, [testCase.appUrl]);
        verify(DragSimulator.drop(strip, at.x, at.y));
        compare(dock.controller.dropped, [String(testCase.appUrl)]);
        compare(strip.dropState, "none");

        DragSimulator.enter(strip, at.x, at.y, [testCase.fileUrl]);
        verify(!DragSimulator.drop(strip, at.x, at.y));
        compare(dock.controller.dropped.length, 1);
    }

    /// An application dropped over a tile is still an application being added,
    /// so the tile declines the enter and the strip answers instead.
    function test_applicationOverATileIsHandledByTheStrip() {
        let dock = makeDock();
        let strip = stripDrop(dock);
        let at = centreOf(dock, 1);

        DragSimulator.enter(strip, at.x, at.y, [testCase.appUrl]);
        compare(tileAt(dock, 1).dropState, "none");
        compare(strip.dropState, "accepted");

        DragSimulator.leave(strip);
    }

    /// A file over a tile is a document being opened, and the answer is the
    /// tile's: a highlight when it will open, a tint and a sentence when not.
    function test_tileShowsTheAnswerForAFileDroppedOnIt() {
        let dock = makeDock();
        let strip = stripDrop(dock);

        let openable = centreOf(dock, 0);
        DragSimulator.enter(strip, openable.x, openable.y, [testCase.fileUrl]);
        compare(tileAt(dock, 0).dropState, "accepted");
        verify(findChild(tileAt(dock, 0), "dropHighlight").visible);
        verify(!refusalLabel(dock).visible);
        DragSimulator.leave(strip);

        // "gamma" is the fake's application that opens nothing.
        let refused = centreOf(dock, 2);
        DragSimulator.enter(strip, refused.x, refused.y, [testCase.fileUrl]);
        compare(tileAt(dock, 2).dropState, "rejected");
        verify(findChild(tileAt(dock, 2), "dropHighlight").visible);
        verify(refusalLabel(dock).visible);
        verify(refusalText(dock).text.length > 0);
        DragSimulator.leave(strip);
    }
}
