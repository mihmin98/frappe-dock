import QtQuick
import QtTest
import org.kde.frappedock
import org.kde.frappedock.test

/*
 * The stack Fan view: entries along an arc, capped rather than scrolled.
 *
 * The assertions are about the shape of the arrangement — entries evenly spaced
 * along a circle, leaning away from the screen edge — rather than about exact
 * coordinates, which would be a restatement of the trigonometry.
 */
Item {
    id: root

    width: 800
    height: 800

    Component {
        id: fanComponent

        StackFan {
            cellSize: 48
            maxItems: 6
        }
    }

    TestCase {
        id: testCase

        name: "StackFan"
        when: windowShown

        function makeFan(rootPath) {
            let model = StackFixture.create(rootPath);
            let fan = fanComponent.createObject(root, { model: model });
            verify(fan, "fan created");
            return fan;
        }

        function ready(fan) {
            StackFixture.setStatus(fan.model, 2); // Ready
            StackFixture.notifyChange(fan.model);
        }

        /// The MouseAreas the Repeater made, in model order.
        function entriesOf(fan) {
            let arc = null;
            for (let i = 0; i < fan.children.length; ++i) {
                if (fan.children[i].objectName === "stackFanArc") {
                    arc = fan.children[i];
                }
            }
            verify(arc, "found the arc");

            let found = [];
            for (let i = 0; i < arc.children.length; ++i) {
                if (arc.children[i] instanceof MouseArea) {
                    found.push(arc.children[i]);
                }
            }
            // Child order is not guaranteed to be model order.
            found.sort((a, b) => a.index - b.index);
            return found;
        }

        /// The Text a delegate draws its name in, or null if it draws none.
        function labelOf(entry) {
            let pending = [entry];
            while (pending.length > 0) {
                let item = pending.shift();
                for (let i = 0; i < item.children.length; ++i) {
                    let child = item.children[i];
                    if (child instanceof Text) {
                        return child;
                    }
                    pending.push(child);
                }
            }
            return null;
        }

        /*
         * Regression — manual checklist 05, run 1, finding 3.
         *
         * The fan drew artwork and nothing else. Grid and List both label their
         * entries, and StackFan went as far as requiring `name` in its delegate
         * before never rendering it, so this was a dropped label rather than a
         * decision. A folder of a dozen documents is a dozen identical page
         * icons without it.
         */
        function test_entriesAreLabelled() {
            let fan = makeFan("/stack/Labels");
            StackFixture.addFile(fan.model, "report.txt");
            StackFixture.addFile(fan.model, "photo.png");
            StackFixture.addDir(fan.model, "archive");
            ready(fan);
            waitForRendering(fan);

            let entries = entriesOf(fan);
            compare(entries.length, 3);
            for (let entry of entries) {
                let label = labelOf(entry);
                verify(label, "entry " + entry.index + " draws a label");
                // Its own name, not a placeholder and not the folder's.
                compare(label.text, entry.name);
                verify(label.visible, "entry " + entry.index + "'s label is visible");
                verify(label.width > 0 && label.height > 0,
                       "entry " + entry.index + "'s label has room to be read");
            }
            fan.destroy();
        }

        /// A label that grew with the longest file name would change the fan's
        /// width every time the folder changed, which is the rule the list view
        /// follows for the same reason.
        function test_labelsElideRatherThanWidenTheFan() {
            // Two fans of the same entry count, differing only in how long the
            // names are. Comparing one fan before and after an insertion would
            // measure the arc growing by an entry, not the labels.
            let short = makeFan("/stack/ShortNames");
            StackFixture.addFile(short.model, "a.txt");
            StackFixture.addFile(short.model, "b.txt");
            ready(short);
            waitForRendering(short);

            let long = makeFan("/stack/LongNames");
            StackFixture.addFile(long.model,
                "a-very-long-file-name-that-goes-on-and-on-and-on-and-on.txt");
            StackFixture.addFile(long.model,
                "another-immoderately-long-file-name-of-the-same-sort.txt");
            ready(long);
            waitForRendering(long);

            compare(long.implicitWidth, short.implicitWidth);
            compare(long.implicitHeight, short.implicitHeight);
            for (let entry of entriesOf(long)) {
                verify(labelOf(entry).elide !== Text.ElideNone, "the label elides");
            }
            short.destroy();
            long.destroy();
        }

        function test_entriesLieOnTheArc() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 5);
            ready(fan);
            waitForRendering(fan);

            let entries = entriesOf(fan);
            compare(entries.length, 5);

            // Entry 0 sits straight above the origin at the full radius, so
            // the origin is recoverable from it, and every other entry then has
            // to be the same distance away — which is what makes the
            // arrangement an arc rather than a diagonal.
            let ox = entries[0].x + entries[0].width / 2;
            let oy = entries[0].y + entries[0].height / 2 + fan.arcRadius;
            for (let i = 1; i < entries.length; ++i) {
                let cx = entries[i].x + entries[i].width / 2;
                let cy = entries[i].y + entries[i].height / 2;
                let r = Math.sqrt((cx - ox) * (cx - ox) + (cy - oy) * (cy - oy));
                fuzzyCompare(r, fan.arcRadius, fan.arcRadius * 0.01);
            }
            fan.destroy();
        }

        function test_entriesAreEvenlySpacedAlongTheArc() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 5);
            ready(fan);
            waitForRendering(fan);

            let entries = entriesOf(fan);
            // One cell pitch of arc between neighbours, which is what makes the
            // fan readable rather than merely curved.
            let expected = fan.cellPitch;
            for (let i = 1; i < entries.length; ++i) {
                let dx = entries[i].x - entries[i - 1].x;
                let dy = entries[i].y - entries[i - 1].y;
                let chord = Math.sqrt(dx * dx + dy * dy);
                // Chord, not arc, so it is slightly shorter than the pitch; 5%
                // covers the curvature at this radius.
                fuzzyCompare(chord, expected, expected * 0.05);
            }
            fan.destroy();
        }

        function test_fanLeansTheSameWayWhateverItsSize() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 3);
            ready(fan);
            waitForRendering(fan);
            let smallSweep = entriesOf(fan)[2].angle;

            StackFixture.addFiles(fan.model, 3);
            StackFixture.notifyChange(fan.model);
            waitForRendering(fan);

            // The radius grows with the entry count rather than the sweep
            // widening, so the third entry sits at a smaller angle than before —
            // a widening sweep would move it somewhere different every time a
            // file landed in the folder.
            verify(entriesOf(fan)[2].angle < smallSweep,
                   "adding entries lengthened the arc rather than widening the sweep");
            verify(entriesOf(fan)[fan.shownCount - 1].angle <= fan.sweep,
                   "the fan stays inside its sweep");
            fan.destroy();
        }

        function test_capsRatherThanCrowds() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 40);
            ready(fan);
            waitForRendering(fan);

            // An arc has a finite amount of room along it. Past the cap the
            // container offers the grid, which is the mode that stays usable.
            compare(fan.shownCount, 6);
            compare(fan.overflowCount, 34);

            let shown = 0;
            for (let entry of entriesOf(fan)) {
                if (entry.visible) {
                    ++shown;
                }
            }
            compare(shown, 6);
            fan.destroy();
        }

        function test_noOverflowWhenEverythingFits() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 4);
            ready(fan);

            compare(fan.shownCount, 4);
            compare(fan.overflowCount, 0);
            fan.destroy();
        }

        function test_fansAwayFromTheScreenEdge() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 4);
            ready(fan);
            waitForRendering(fan);
            let rightwards = entriesOf(fan)[3].x - entriesOf(fan)[0].x;
            verify(rightwards > 0, "a bottom dock fans to the right");

            fan.dockPosition = ConfigFacade.Right;
            waitForRendering(fan);
            // A dock on the right edge has to fan the other way or the entries
            // walk off the screen.
            verify(entriesOf(fan)[3].x - entriesOf(fan)[0].x < 0, "a right dock fans to the left");
            fan.destroy();
        }

        function test_drillDownAndReturn() {
            let fan = makeFan("/stack/Root");
            StackFixture.addDir(fan.model, "inner");
            StackFixture.addFile(fan.model, "outer.txt");
            ready(fan);

            verify(fan.model.enterFolder(0), "row 0 is the folder");
            compare(fan.model.path, "/stack/Root/inner");
            verify(fan.model.goUp(), "back up");
            compare(fan.model.path, "/stack/Root");
            fan.destroy();
        }

        function test_statesAreDistinct() {
            let fan = makeFan("/stack/Root");
            ready(fan);
            compare(fan.empty, true);

            StackFixture.setStatus(fan.model, 3); // Failed
            StackFixture.notifyChange(fan.model);
            compare(fan.failed, true);
            compare(fan.empty, false);
            fan.destroy();
        }

        function test_proportionsFollowCellSize() {
            let fan = makeFan("/stack/Root");
            StackFixture.addFiles(fan.model, 4);
            ready(fan);

            let small = { r: fan.arcRadius, w: fan.implicitWidth, h: fan.implicitHeight };
            fan.cellSize = 96;

            fuzzyCompare(fan.arcRadius, small.r * 2, small.r * 0.01);
            fuzzyCompare(fan.implicitWidth, small.w * 2, small.w * 0.01);
            fuzzyCompare(fan.implicitHeight, small.h * 2, small.h * 0.01);
            fan.destroy();
        }
    }
}
