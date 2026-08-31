pragma ComponentBehavior: Bound

import QtQuick
import org.kde.frappedock

/*
 * The dock shelf.
 *
 * Every dimension here derives from one number, `tileSize` — S in the
 * proportion model in plan.md Part 0. A dock at S = 24 must be a uniform scale
 * of a dock at S = 128, so there are no numeric literals below beyond the
 * model's own ratios. tst_tile.qml's proportionsFollowTileSize() is what keeps
 * that true.
 *
 * Phase 3 replaces tile *positions* with geometry-engine output. It does not
 * replace these proportions.
 */
Item {
    id: dock

    /// The TileModel, injected by the surface factory (or by a test).
    property var tileModel: null

    /// The DockController, injected the same way. Null in tests that only care
    /// about layout; popups are then simply not offered.
    property var controller: null

    /// Emitted when a tile is activated. main() routes these to the controller,
    /// which dispatches them through the binding matrix.
    signal tileClicked(string tileId, int button, int modifiers)
    signal tileHeld(string tileId)

    // --- The proportion model -------------------------------------------
    readonly property real iconSize: FrappeConfig.tileSize      // S, the layout cell
    // S/3. The ratio comes from GeometryTuning rather than being written here,
    // so the harness moves the gap and the shelf's padding together — they are
    // the same constant, and tuning one without the other would be tuning a
    // dock that cannot exist.
    readonly property real gap: iconSize * GeometryTuning.spacingRatio
    readonly property real thickness: iconSize + 2 * gap        // 5S/3
    readonly property real shelfRadius: 0.28 * thickness        // 0.28 x thickness

    /// The dock floats: g/3 between the shelf and the screen edge. It used to be
    /// a layer-shell margin, but the surface now spans the whole output so that
    /// the drag-out affordance has somewhere to go — which makes the gap this
    /// item's business rather than the compositor's.
    readonly property real screenGap: gap / 3

    /// Where the shelf's near edge sits across the dock's axis. Everything that
    /// needs to know where the shelf *is* — as opposed to how thick it is —
    /// comes from here. On a surface the size of the screen, "against the edge"
    /// is no longer the same as "at the item's edge".
    readonly property real shelfCross: horizontal
        ? height - screenGap - thickness
        : (FrappeConfig.position === ConfigFacade.Left
           ? screenGap : width - screenGap - thickness)

    /// What the *surface* has to be, which is not the shelf's thickness: a
    /// magnified tile grows out of the shelf and away from the screen edge, and
    /// a tile dragged out to be removed goes further still. The shelf itself is
    /// normative geometry and does not grow with either.
    ///
    /// Read from the engine rather than recomputed here. The layer surface is
    /// sized from the same core function, and the two disagreeing is what left
    /// the drag-out gesture drawing into space the surface did not have.
    readonly property real surfaceThickness: geometry.surfaceThickness
    // --------------------------------------------------------------------

    /// Cell pitch: the distance between the leading edges of two adjacent tiles.
    /// At rest this is what the engine produces; it stays exposed for tests.
    readonly property real cellPitch: iconSize + gap

    /// Which rows are separators. TileModel reports them; a plain list model in
    /// a test has none, and passing an empty list is the honest answer there.
    readonly property var separatorRows: tileModel && tileModel.separatorRows
                                         ? tileModel.separatorRows() : []

    /// The usable length along the dock's axis: what the shelf may grow to
    /// before the engine starts compressing. It is read from the output rather
    /// than from this item's own size, because the shelf is sized *by* the
    /// engine — feeding its width back in is a binding loop. Zero means "not
    /// known yet", which the engine reads as unconstrained.
    property real availableLength: (horizontal ? Screen.width : Screen.height)
                                   - 2 * (gap / 3)

    readonly property bool horizontal: FrappeConfig.position === ConfigFacade.Bottom

    /// The engine's origin, in this item's coordinates: where the shelf's
    /// leading edge sits with the pointer nowhere near it. Everything the
    /// engine says is measured from here, including the pointer — and it is
    /// deliberately derived from the *resting* length rather than the current
    /// one, because an origin that moved with the shelf would feed the shelf's
    /// own position back into the pointer that decides it.
    readonly property real restingOrigin:
        ((horizontal ? width : height) - geometry.restingLength) / 2

    // --- Motion ----------------------------------------------------------
    //
    // Pointer motion already arrives as a continuous stream, so the transitions
    // worth animating are the discontinuous ones: the pointer entering the
    // shelf, leaving it, and the strip reflowing when a window opens or closes.
    // A `SmoothedAnimation` is what handles both without a mode switch — it
    // retargets mid-flight instead of restarting, so a value that changes on
    // every frame is followed rather than repeatedly interrupted, which is
    // where jank on a per-motion-event `NumberAnimation` comes from.
    //
    // 130 ms is the settle; the follow reads as immediate because the target is
    // never more than a frame away.
    readonly property int baseDuration: 130

    /// Durations scaled by the speed setting. 0 disables animation outright,
    /// which is also what a reduced-motion preference will drive (Phase 8).
    readonly property bool animated: FrappeConfig.animationSpeed > 0
    readonly property int duration: animated ? Math.round(baseDuration * 100 / FrappeConfig.animationSpeed) : 0

    /// False until the first layout has been applied. An animation is for
    /// moving between two states the user has seen; the arrival of the *first*
    /// state is not a transition, and animating it makes the dock assemble
    /// itself on screen every time it starts. `Qt.callLater` is what draws the
    /// line: it runs after the current binding pass, by which point every
    /// delegate exists and the engine has published a layout covering all of
    /// them.
    property bool settled: false
    Component.onCompleted: Qt.callLater(() => dock.settled = true)
    // --------------------------------------------------------------------

    /// The shelf's own thickness. The gap to the screen edge is a layer-shell
    /// margin on the surface, not padding here, so the shelf fills its window on
    /// the anchored axis.
    /// Sized to the *resting* shelf: on a real surface this item spans the
    /// screen edge and its size is the compositor's business, but where it is
    /// asked for an implicit size — a test — it must be a fixed one. Sizing to
    /// the magnified shelf would make the item grow as the pointer moves, and
    /// `restingOrigin` is measured from that item.
    /// Includes the screen gap, which the shelf now sits inside rather than the
    /// compositor supplying as a margin — so the headroom above the shelf is
    /// still exactly `surfaceThickness` less the shelf itself.
    implicitWidth: horizontal ? geometry.restingLength : surfaceThickness + screenGap
    implicitHeight: horizontal ? surfaceThickness + screenGap : geometry.restingLength

    // --- Reordering ------------------------------------------------------
    //
    // The drag moves the *model* as it goes rather than shuffling delegates:
    // the strip is laid out from the model's order, so a live move is already a
    // live reflow, and there is no second, temporary ordering to reconcile on
    // drop. Persistence comes free with it — moveTile writes the order to
    // config — which is why there is nothing to do on release beyond letting go.

    /// Which row is being dragged, or -1. It follows the tile as the move
    /// happens, so it is the row's *current* index, not the one it started at.
    property int draggingRow: -1

    /// Where the pointer is, in this item's coordinates. The dragged tile is
    /// drawn from it directly so it tracks the pointer rather than the layout
    /// it is disturbing.
    property point dragPointer: Qt.point(0, 0)

    /// Whether the row being dragged is a pinned one. Captured when the drag
    /// starts: only a pinned tile has anything to remove, and its pinned state
    /// cannot change mid-gesture.
    property bool dragIsPinned: false

    readonly property real dragPointerAxis: dock.horizontal ? dock.dragPointer.x : dock.dragPointer.y

    /// How far the pointer is *outside* the shelf, across the dock's axis.
    /// Negative while it is still over the shelf.
    readonly property real dragDistanceFromShelf:
        dock.horizontal ? dock.shelfCross - dock.dragPointer.y
                        : (FrappeConfig.position === ConfigFacade.Left
                           ? dock.dragPointer.x - (dock.shelfCross + dock.thickness)
                           : dock.shelfCross - dock.dragPointer.x)

    /// How far out the tile has to be dragged before the release unpins it.
    /// One tile: far enough that it cannot be reached by a hand aiming at the
    /// shelf, close enough to be an easy deliberate gesture — and, being a
    /// multiple of S, the same gesture at every dock size.
    readonly property real removeThreshold: iconSize

    readonly property bool reorderable: dock.tileModel && dock.tileModel.moveTile !== undefined
    readonly property bool removable: dock.tileModel && dock.tileModel.unpinTile !== undefined

    /// True when releasing now would unpin. This is what the affordance shows,
    /// and it is deliberately live: dragging back over the shelf takes it away
    /// again, so the gesture can be called off by reversing it.
    readonly property bool dragWillRemove: dock.draggingRow >= 0 && dock.removable
                                           && dock.dragIsPinned
                                           && dock.dragDistanceFromShelf > dock.removeThreshold

    function beginTileDrag(row, scenePosition, isPinned) {
        if (!dock.reorderable) {
            return;
        }
        dock.dragIsPinned = isPinned;
        dock.draggingRow = row;
        dock.updateTileDrag(scenePosition);
    }

    function updateTileDrag(scenePosition) {
        if (dock.draggingRow < 0) {
            return;
        }

        dock.dragPointer = dock.mapFromItem(null, scenePosition);

        // Out past the threshold the gesture is a removal, not a reorder.
        // Reflowing the strip underneath it would be answering a question the
        // user has stopped asking, and would leave the tile somewhere new if
        // they then dragged back in to cancel.
        if (dock.dragWillRemove) {
            return;
        }

        // The same placements the view drew from decide the target row, so the
        // tile lands where it looks like it will.
        let target = geometry.rowAt(dock.dragPointerAxis - dock.restingOrigin);
        if (target < 0 || target === dock.draggingRow) {
            return;
        }
        // A separator is a rule, not a slot: dropping onto one would ask the
        // model to order a tile against something that has no order.
        if (dock.separatorRows.indexOf(target) !== -1) {
            return;
        }

        if (dock.tileModel.moveTile(dock.draggingRow, target)) {
            dock.draggingRow = target;
        }
    }

    function endTileDrag() {
        // Unpinning only ever edits the pinned list. The application stays
        // installed and launchable from everywhere else — a dock that could
        // uninstall by gesture would be one slip away from real damage.
        if (dock.dragWillRemove) {
            dock.tileModel.unpinTile(dock.draggingRow);
        }
        dock.draggingRow = -1;
        dock.dragIsPinned = false;
    }
    // --------------------------------------------------------------------

    // --- Drop feedback ---------------------------------------------------
    //
    // Why a drop will be refused, and over which tile. The tile decides the
    // wording — it is the one that asked — but the message is shown here,
    // because a cell is one tile wide and a sentence is not.

    /// The row the current refusal is about, or -1 when it is about the strip
    /// as a whole rather than one tile.
    property int dropRefusalRow: -1
    property string dropRefusal: ""
    /// Where along the axis a strip-level refusal is pointing, or -1.
    property real dropRefusalPointerAxis: -1

    function showStripFeedback(axis, refusal) {
        dock.dropRefusalRow = -1;
        dock.dropRefusalPointerAxis = axis;
        dock.dropRefusal = refusal;
    }

    function clearStripFeedback() {
        if (dock.dropRefusalRow === -1) {
            dock.dropRefusal = "";
            dock.dropRefusalPointerAxis = -1;
        }
    }

    function showDropRefusal(row, refusal) {
        if (refusal.length > 0) {
            dock.dropRefusalPointerAxis = -1;
            dock.dropRefusalRow = row;
            dock.dropRefusal = refusal;
        } else if (dock.dropRefusalRow === row) {
            // Only the tile that raised the message may clear it: the drag
            // leaving one tile for the next arrives as an exit and an enter,
            // in that order, and clearing unconditionally would swallow the
            // new message.
            dock.dropRefusalRow = -1;
            dock.dropRefusal = "";
        }
    }
    // --------------------------------------------------------------------

    /// Routes one interaction: opens a popup here, or hands it to C++.
    ///
    /// \a row is the tile the interaction was on, so a popup can be placed
    /// against it. A menu opened without an anchor resolves the pointer against
    /// its parent, which on a layer-shell surface it cannot do — it lands at the
    /// origin, and on a shelf that spans the screen that is the far edge.
    function handle(id, button, modifiers, held, row) {
        let command = dock.controller ? dock.controller.commandFor(button, modifiers, held)
                                      : DockCommand.LaunchOrActivate;

        if (command === DockCommand.ShowContextMenu || command === DockCommand.ShowJumpList) {
            menu.openFor(id, repeater.itemAt(row));
            return;
        }

        if (held) {
            dock.tileHeld(id);
        } else {
            dock.tileClicked(id, button, modifiers);
        }
    }

    /*
     * The geometry engine. Tile positions and sizes come from here and from
     * nowhere else — there is deliberately no second path that computes them
     * from the proportion model directly, because a fallback that disagrees
     * with the engine is exactly how hit regions and layout drift apart
     * (plan.md §3.4).
     */
    DockGeometry {
        id: geometry
        objectName: "geometry"

        tileCount: repeater.count
        tileSize: dock.iconSize
        availableLength: dock.availableLength
        separatorRows: dock.separatorRows
        spacingRatio: GeometryTuning.spacingRatio

        // Config is the only source of these: the tuning harness moves the same
        // keys the settings page will, so there is nothing to migrate when
        // Phase 7 puts a UI on them.
        magnificationEnabled: FrappeConfig.magnificationEnabled
        magnifiedSize: FrappeConfig.magnificationFactor * dock.iconSize
        falloffRadius: FrappeConfig.falloffRadius
        curveExponent: FrappeConfig.curveExponent

        // The handler reports in this item's coordinates; the engine measures
        // from the resting shelf's leading edge. Skipping this translation
        // works only where the dock is exactly shelf-sized — which is true in a
        // test and false on a real screen-spanning surface.
        // A drag takes the pointer grab, and a HoverHandler stops reporting once
        // it does — so during a reorder the drag's own position is the only
        // live one. Without this the strip freezes where the drag began while
        // the tile goes on tracking the pointer, and the drop target, which is
        // read from these same placements, stops agreeing with what is drawn.
        pointerPosition: dock.draggingRow >= 0
            ? dock.dragPointerAxis - dock.restingOrigin
            : (hover.hovered
               ? (dock.horizontal ? hover.point.position.x : hover.point.position.y) - dock.restingOrigin
               : -1)
    }

    HoverHandler {
        id: hover
        objectName: "hover"
        // The hot path: every motion event lands here and re-runs the engine.
        target: shelf
    }

    TileMenu {
        id: menu
        objectName: "tileMenu"
        controller: dock.controller
        dockPosition: FrappeConfig.position
    }

    /// The centre of the tile the refusal is about, along the dock's axis, in
    /// this item's coordinates. Read from the engine like everything else, so
    /// the message stays over its tile while the strip magnifies underneath.
    readonly property real dropRefusalAxis:
        (dropRefusalRow >= 0 && dropRefusalRow < geometry.tileGeometry.length)
            ? restingOrigin + geometry.tileGeometry[dropRefusalRow].offset
              + geometry.tileGeometry[dropRefusalRow].size / 2
            : Math.max(0, dropRefusalPointerAxis)

    /*
     * Why the drop will be refused, in words.
     *
     * The design correction in §4.4: a target that cannot take what is being
     * offered says so, rather than silently declining and leaving the user to
     * guess. The tile's tint says *that* it will be refused; this says why.
     */
    Rectangle {
        id: dropRefusalLabel
        objectName: "dropRefusalLabel"

        visible: dock.dropRefusal.length > 0
        z: 2

        width: refusalText.implicitWidth + 2 * dock.gap
        height: refusalText.implicitHeight + dock.gap
        radius: height / 2

        // Just clear of the shelf, on the inner side. Measured from where the
        // shelf is rather than from this item's edge — the two stopped being
        // the same thing when the surface grew to the whole output.
        x: dock.horizontal
           ? Math.max(0, Math.min(dock.width - width, dock.dropRefusalAxis - width / 2))
           : (FrappeConfig.position === ConfigFacade.Left
              ? dock.shelfCross + dock.thickness + dock.gap
              : dock.shelfCross - dock.gap - width)
        y: dock.horizontal
           ? dock.shelfCross - dock.gap - height
           : Math.max(0, Math.min(dock.height - height, dock.dropRefusalAxis - height / 2))

        color: Qt.rgba(0, 0, 0, 0.7)
        border.width: 1
        border.color: Qt.rgba(1, 0.3, 0.3, 0.8)

        Text {
            id: refusalText
            objectName: "dropRefusalText"
            anchors.centerIn: parent
            text: dock.dropRefusal
            color: "white"
        }
    }

    /*
     * The Remove affordance.
     *
     * Shown only while releasing would actually unpin, so it is a statement of
     * what will happen rather than a hint that it might. It says "Remove" in
     * words: a gesture that quietly changes the dock's contents should name
     * itself, and an unlabelled puff of smoke leaves the user to find out
     * afterwards.
     */
    Rectangle {
        id: removeAffordance
        objectName: "removeAffordance"

        visible: dock.dragWillRemove
        // Above the shelf and the tile being dragged out of it.
        z: 2

        // Sized to its label, padded on the proportion model like everything
        // else, and placed just beyond the tile on the way out.
        width: removeLabel.implicitWidth + 2 * dock.gap
        height: removeLabel.implicitHeight + dock.gap
        radius: height / 2

        x: dock.horizontal
           ? Math.max(0, Math.min(dock.width - width, dock.dragPointerAxis - width / 2))
           : (FrappeConfig.position === ConfigFacade.Left
              ? dock.dragPointer.x + dock.gap
              : dock.dragPointer.x - dock.gap - width)
        y: dock.horizontal ? dock.dragPointer.y - dock.iconSize - height
                           : Math.max(0, Math.min(dock.height - height, dock.dragPointerAxis - height / 2))

        color: Qt.rgba(0, 0, 0, 0.6)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.3)

        Text {
            id: removeLabel
            objectName: "removeLabel"
            anchors.centerIn: parent
            // Colour and type are Phase 6's business; the wording is not.
            text: qsTr("Remove")
            color: "white"
        }
    }

    Rectangle {
        id: shelf
        objectName: "shelf"

        // Along the dock's axis the shelf is placed by the engine, not centred:
        // magnification anchors the pointed tile under the cursor, so the strip
        // grows in whichever direction the pointer is and the shelf has to
        // follow it.
        //
        // Across the axis it sits against the screen edge rather than centred,
        // because the surface is thicker than the shelf — the surplus is
        // headroom for magnified tiles and belongs entirely on the inner side.
        x: dock.horizontal ? dock.restingOrigin + geometry.shelfStart : dock.shelfCross
        y: dock.horizontal ? dock.shelfCross : dock.restingOrigin + geometry.shelfStart

        // Shelf padding equals the inter-icon gap — one constant, used in both
        // places. This is not a coincidence to be re-derived; it falls out of the
        // fit Part 0 chooses, and the engine has already applied it at each end
        // of `shelfLength`.
        width: dock.horizontal ? geometry.shelfLength : dock.thickness
        height: dock.horizontal ? dock.thickness : geometry.shelfLength

        radius: dock.shelfRadius

        // The shelf grows with the strip it holds, and has to do it on the same
        // curve or the tiles visibly outrun their own container. It moves for
        // the same reason and on the same curve: an animated width against an
        // instant x would make the shelf lurch and then catch up.
        Behavior on x {
            enabled: dock.animated && dock.settled && dock.horizontal
            SmoothedAnimation { velocity: -1; duration: dock.duration }
        }
        Behavior on y {
            enabled: dock.animated && dock.settled && !dock.horizontal
            SmoothedAnimation { velocity: -1; duration: dock.duration }
        }
        Behavior on width {
            enabled: dock.animated && dock.settled && dock.horizontal
            SmoothedAnimation { velocity: -1; duration: dock.duration }
        }
        Behavior on height {
            enabled: dock.animated && dock.settled && !dock.horizontal
            SmoothedAnimation { velocity: -1; duration: dock.duration }
        }

        // Colour and material are Phase 6's business, delegated to the blur
        // effect and the colour scheme. What is normative here is the shape and
        // the rim stroke's existence and 1 pt width, which does not scale.
        color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
        // The rim states what a drop onto the shelf would do: it is the whole
        // shelf that is being aimed at, so it is the whole shelf that answers.
        border.color: stripDrop.dropState === "accepted" ? Qt.rgba(1, 1, 1, 0.8)
                      : stripDrop.dropState === "rejected" ? Qt.rgba(1, 0.3, 0.3, 0.8)
                      : Qt.rgba(1, 1, 1, 0.2)

        /*
         * Spring-loading: a drag left resting on a tile activates it, so the
         * user can drop into the application's own window without putting the
         * drag down first. The delay and the once-per-drag rule are the
         * loader's; the view only reports what the pointer did.
         */
        SpringLoader {
            id: springLoader
            objectName: "springLoader"

            delay: FrappeConfig.springLoadDelay
            onSpringLoaded: (tileId) => {
                if (dock.controller) {
                    dock.controller.activateTile(tileId);
                }
            }
        }

        /*
         * Drops onto the strip itself — beside the tiles rather than onto one.
         *
         * This is where an application is added to the dock, and where the
         * region rules apply: applications belong among the pinned tiles, files
         * and folders in the file region. A target that cannot take what is
         * being offered says so; §4.4's design correction is that macOS does
         * not.
         */
        DropArea {
            id: stripDrop
            objectName: "stripDrop"

            anchors.fill: parent
            keys: ["text/uri-list"]

            /// The controller's answer for the drag overhead, or null.
            property var verdict: null
            /// "none", "accepted" or "rejected". Not called `state`: that name
            /// belongs to Item's state machine.
            readonly property string dropState: !containsDrag ? "none"
                                                : (verdict && verdict.accepted ? "accepted" : "rejected")

            /// Which region the pointer is over. The model reports it; a model
            /// that does not answer has one region, which is the honest reading
            /// of a flat list.
            function regionAt(axis) {
                let row = geometry.rowAt(axis - dock.restingOrigin);
                return (dock.tileModel && dock.tileModel.regionOfRow && row >= 0)
                       ? dock.tileModel.regionOfRow(row) : TileRegion.Pinned;
            }

            function evaluate(drag) {
                let axis = dock.horizontal ? shelf.x + drag.x : shelf.y + drag.y;
                stripDrop.verdict = dock.controller
                    ? dock.controller.evaluateRegionDrop(stripDrop.regionAt(axis), drag.urls) : null;
                dock.showStripFeedback(axis, stripDrop.refusalText());
            }

            function refusalText() {
                let v = stripDrop.verdict;
                if (!v || v.accepted) {
                    return "";
                }
                switch (v.rejection) {
                case RegionDropRejection.WrongRegion:
                    return v.expectedRegion === TileRegion.Files
                        ? qsTr("Files and folders don't go in the applications area")
                        : qsTr("Applications don't go in the file area");
                case RegionDropRejection.MixedPayload:
                    return qsTr("Drop applications and files separately");
                case RegionDropRejection.NotInstalled:
                    return qsTr("%1 isn't an installed application").arg(v.detail);
                default:
                    return qsTr("This can't be added to the dock");
                }
            }

            // Accepted whatever the answer, so that the answer can be shown.
            // A DropArea that refuses the enter stops hearing about the drag.
            onEntered: (drag) => {
                stripDrop.evaluate(drag);
                drag.accepted = true;
            }
            onPositionChanged: (drag) => stripDrop.evaluate(drag)

            onExited: {
                stripDrop.verdict = null;
                dock.clearStripFeedback();
                // Reported rather than acted on: this exit also fires when the
                // drag merely crosses onto a tile, whose drop area is nested
                // inside this one. The loader settles which happened.
                springLoader.dockLeft();
            }

            onDropped: (drop) => {
                let axis = dock.horizontal ? shelf.x + drop.x : shelf.y + drop.y;
                drop.accepted = dock.controller
                    ? dock.controller.acceptRegionDrop(stripDrop.regionAt(axis), drop.urls) : false;
                stripDrop.verdict = null;
                dock.clearStripFeedback();
                springLoader.dragFinished();
            }
        }

        Item {
            id: row
            objectName: "row"

            // The engine's offsets are measured from the shelf's outer edge and
            // already include the inner padding, so the strip is the shelf and
            // nothing is added on either side. Insetting it by `gap` here would
            // count that padding twice, and would be wrong the moment the
            // layout compresses and the padding shrinks with the tiles.
            anchors.fill: parent

            Repeater {
                id: repeater
                objectName: "repeater"
                model: dock.tileModel

                delegate: Tile {
                    required property string tileId
                    required property string name
                    required property string iconName
                    required property int kind
                    // Tile already declares these two under the same names the
                    // model roles use, so they are marked required rather than
                    // re-declared, and the Repeater fills them directly.
                    required isRunning
                    required windowCount
                    /// Only a pinned tile has anything to unpin, so the drag-out
                    /// gesture needs to know before it offers to.
                    required property bool isPinned

                    required property int index

                    /// Placement for this row, straight from the engine.
                    /// Guarded because the Repeater instantiates delegates
                    /// before the geometry list has caught up with the count —
                    /// and, when a row is removed, keeps a delegate alive for a
                    /// moment with `index` already set to -1.
                    readonly property var placement: (index >= 0 && index < geometry.tileGeometry.length)
                                                     ? geometry.tileGeometry[index]
                                                     : ({ offset: 0, size: dock.iconSize })

                    /// Where the cell starts along the dock's axis. A separator
                    /// is given the whole gap it sits in rather than a cell, so
                    /// it is centred in that span; a tile starts where the
                    /// engine put it.
                    /// Less `shelfStart`, because these are placed inside the
                    /// shelf and the shelf itself has moved.
                    readonly property real axisOffset: (isSeparator
                        ? placement.offset + (placement.size - (dock.horizontal ? width : height)) / 2
                        : placement.offset) - geometry.shelfStart

                    // The engine positions along the dock's axis. Across it, the
                    // tile keeps its outer edge on the resting icon's line —
                    // one gap in from the shelf's outer edge — and grows away
                    // from the screen edge. Centring it on the shelf instead
                    // sends half the growth into the screen edge, where there
                    // is no room to be had at any surface thickness.
                    /// True while this row is the one under the finger.
                    readonly property bool dragging: dock.draggingRow === index

                    /// The dragged tile is centred on the pointer instead of
                    /// on its cell, so it stays under the finger while the
                    /// cells beneath it reflow. Measured in `row` coordinates,
                    /// which is what the shelf's own offset takes out.
                    readonly property real dragAxis:
                        dock.dragPointerAxis - dock.restingOrigin - geometry.shelfStart
                        - (dock.horizontal ? width : height) / 2

                    /// Once the tile is out past the removal threshold it stops
                    /// being a cell in the strip and follows the pointer on both
                    /// axes, so the user can see it is no longer in the dock.
                    /// In `row` coordinates, which the shelf's own placement
                    /// across the axis takes out.
                    readonly property real dragCross:
                        (dock.horizontal ? dock.dragPointer.y - dock.shelfCross
                                         : dock.dragPointer.x - dock.shelfCross)
                        - (dock.horizontal ? height : width) / 2

                    readonly property bool leavingDock: dragging && dock.dragWillRemove

                    readonly property real restingCross: dock.horizontal
                        ? row.height - dock.gap - height
                        : (FrappeConfig.position === ConfigFacade.Left
                           ? dock.gap : row.width - dock.gap - width)

                    x: dock.horizontal ? (dragging ? dragAxis : axisOffset)
                                       : (leavingDock ? dragCross : restingCross)
                    y: dock.horizontal ? (leavingDock ? dragCross : restingCross)
                                       : (dragging ? dragAxis : axisOffset)

                    // Dimmed on the way out: the tile is still under the finger
                    // and still returnable, so it fades rather than vanishing.
                    opacity: leavingDock ? 0.5 : 1

                    // Above the tiles it is being dragged past, and never
                    // smoothed: an animated position would lag the pointer.
                    z: dragging ? 1 : 0

                    // A separator's placement is its clearance, not a cell, so
                    // it takes the size the strip settled on instead.
                    //
                    // The dragged tile is held at its resting size. It has left
                    // the strip — it is centred on the pointer, above the cells,
                    // and on its way out of the dock — so sizing it from the
                    // magnification curve makes the thing in the user's hand
                    // grow and shrink according to where the layout thinks it
                    // still is. The tiles it passes go on magnifying normally.
                    iconSize: dragging ? dock.iconSize
                                       : (isSeparator ? geometry.effectiveTileSize : placement.size)

                    // Size and position are animated; nothing else is. All
                    // three run on the scene graph as property animations, so
                    // the work is on the render thread rather than stepped in
                    // JavaScript.
                    Behavior on x {
                        enabled: dock.animated && dock.settled && dock.horizontal && !dragging
                        SmoothedAnimation { velocity: -1; duration: dock.duration }
                    }
                    Behavior on y {
                        enabled: dock.animated && dock.settled && !dock.horizontal && !dragging
                        SmoothedAnimation { velocity: -1; duration: dock.duration }
                    }
                    Behavior on iconSize {
                        enabled: dock.animated && dock.settled
                        SmoothedAnimation { velocity: -1; duration: dock.duration }
                    }
                    gap: dock.gap
                    horizontal: dock.horizontal
                    dockPosition: FrappeConfig.position

                    entryId: tileId
                    entryName: name
                    entryIcon: iconName
                    entryKind: kind

                    // Popups have to open on the surface that was clicked, so
                    // the view asks the matrix what the input means and keeps
                    // those two commands for itself. Everything else goes to
                    // C++ as before; the matrix is still the only place the
                    // mapping is written down.
                    onActivated: (id, button, modifiers) => dock.handle(id, button, modifiers, false, index)
                    onHeld: (id) => dock.handle(id, Qt.LeftButton, Qt.NoModifier, true, index)

                    // For file drops: the tile asks whether the files it is
                    // being offered can be opened, and then asks for them to be.
                    controller: dock.controller

                    /// The refusal message goes to the dock, which has room for
                    /// a sentence, and is anchored to the tile it is about.
                    onDropFeedbackChanged: (refusal) => dock.showDropRefusal(index, refusal)

                    /// Spring-loading: the tile reports the hover, the dock
                    /// times it, and the controller carries it out.
                    springArmed: springLoader.armedTile === tileId
                    onDragHovered: (id) => springLoader.dragEntered(id)
                    onDragHoverEnded: springLoader.dragLeft()
                    onDragReleased: springLoader.dragFinished()

                    onDragStarted: (scenePosition) => dock.beginTileDrag(index, scenePosition, isPinned)
                    onDragMoved: (scenePosition) => dock.updateTileDrag(scenePosition)
                    onDragFinished: dock.endTileDrag()
                }
            }
        }
    }
}
