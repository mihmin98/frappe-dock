# Manual checklist 04 — reordering

`test_tilemodel_reorder` proves the model reorders and writes the new order to config, and
`tst_reorder.qml` proves the gesture drives it with synthetic events. Neither can prove that
a **real pointer on a real surface** feels like dragging a tile, and neither restarts the
process — persistence is tested in-process, against a second facade on the same file.

Everything here runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

## Checks

- [ Y ] Press a tile and move the pointer along the dock. The tile lifts and follows the pointer;
      it does not lag behind it or snap between cells.
- [ Y ] While the tile is still held, the tiles it passes **reflow out of the way**. The reflow
      happens during the drag, not on release.
- [ Y ] Release. The tile settles into the cell the gap was showing, not one over.
- [ Y ] Drag back to the original position and release. The strip is as it started.
- [ N (the magnification does not follow the cursor) ] Drag a tile with **magnification enabled**. The tile still tracks the pointer, and the
      drop lands where the gap was — magnification changes tile sizes under the drag, and the
      drop target is read from the same magnified placements the view drew.
- [ ] Drag with the dock on the **left** and on the **right** edge (`position` in settings).
      The drag axis is vertical and behaves identically.
- [ Y ] A plain click still launches. A drag does **not** also launch the application.
- [ Y ] Press and hold without moving still opens the context menu.
- [ Y ] Drag a **running but unpinned** application about. It moves; the pinned list in
      `frappe-dockrc` is unchanged.
- [ Y ] Drag past the end of the shelf and release. The tile lands in the last cell; nothing is
      removed. (Drag-out-to-remove is §4.2 — until it lands, leaving the shelf must be inert.)

## Persistence across a restart

- [ ] Reorder two pinned tiles. Quit the dock (Ctrl-C in the terminal running it).
- [ ] `grep -i pinned /tmp/frappe-run/config/frappe-dockrc` shows the new order.
- [ ] Start it again with the same command. The dock comes up in the reordered state.

Verdict: Persistence does not work
