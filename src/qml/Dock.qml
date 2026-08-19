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

    /// The peak a tile can reach. Not a dimension in the proportion model — a
    /// multiple of one — which is why it is a ratio in the config.
    readonly property real peakSize: FrappeConfig.magnificationEnabled
                                     ? FrappeConfig.magnificationFactor * iconSize : iconSize

    /// What the *surface* has to be, which is not the shelf's thickness: a
    /// magnified tile grows out of the shelf and away from the screen edge, and
    /// at the maximum peak it is several times as thick as the shelf. The shelf
    /// itself is normative geometry and does not grow with the setting.
    readonly property real surfaceThickness: Math.max(thickness, gap + peakSize)
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
    implicitWidth: horizontal ? geometry.restingLength : surfaceThickness
    implicitHeight: horizontal ? surfaceThickness : geometry.restingLength

    /// Routes one interaction: opens a popup here, or hands it to C++.
    function handle(id, button, modifiers, held) {
        let command = dock.controller ? dock.controller.commandFor(button, modifiers, held)
                                      : DockCommand.LaunchOrActivate;

        if (command === DockCommand.ShowContextMenu || command === DockCommand.ShowJumpList) {
            menu.openFor(id);
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
        pointerPosition: hover.hovered
            ? (dock.horizontal ? hover.point.position.x : hover.point.position.y) - dock.restingOrigin
            : -1
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
        x: dock.horizontal ? dock.restingOrigin + geometry.shelfStart
                           : (FrappeConfig.position === ConfigFacade.Left ? 0 : dock.width - width)
        y: dock.horizontal ? dock.height - height : dock.restingOrigin + geometry.shelfStart

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
        border.color: Qt.rgba(1, 1, 1, 0.2)

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

                    required property int index

                    /// Placement for this row, straight from the engine.
                    /// Guarded because the Repeater instantiates delegates
                    /// before the geometry list has caught up with the count.
                    readonly property var placement: index < geometry.tileGeometry.length
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
                    x: dock.horizontal ? axisOffset
                                       : (FrappeConfig.position === ConfigFacade.Left
                                          ? dock.gap : row.width - dock.gap - width)
                    y: dock.horizontal ? row.height - dock.gap - height : axisOffset

                    // A separator's placement is its clearance, not a cell, so
                    // it takes the size the strip settled on instead.
                    iconSize: isSeparator ? geometry.effectiveTileSize : placement.size

                    // Size and position are animated; nothing else is. All
                    // three run on the scene graph as property animations, so
                    // the work is on the render thread rather than stepped in
                    // JavaScript.
                    Behavior on x {
                        enabled: dock.animated && dock.settled && dock.horizontal
                        SmoothedAnimation { velocity: -1; duration: dock.duration }
                    }
                    Behavior on y {
                        enabled: dock.animated && dock.settled && !dock.horizontal
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
                    onActivated: (id, button, modifiers) => dock.handle(id, button, modifiers, false)
                    onHeld: (id) => dock.handle(id, Qt.LeftButton, Qt.NoModifier, true)
                }
            }
        }
    }
}
