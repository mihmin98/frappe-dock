# Manual checklist 04 — spring-loading

`test_springload` proves the rules: fires at the threshold, cancels on an early exit or a drop,
once per tile per drag, off at zero. `tst_springload.qml` proves the view is wired to them.

Neither can answer the two questions that decide whether this behaviour is worth having:
**does the delay feel right**, and **are accidental triggers rare**? Spring-loading is the only
dock interaction that happens without a release, so a wrong delay is not a small annoyance —
too short and the dock acts on drags the user was only routing past it, too long and they give
up and wait for nothing.

The default is 700 ms (`springLoadDelay`). If this checklist says it is wrong, change the
default; that is what the checklist is for.

Everything runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

Pin a text editor, an image viewer and a terminal emulator. Open Dolphin in the nested session
for the drags, with the dock's full width in reach.

## It fires when it should

- [ Y ] Drag a file onto the **editor's tile** and hold still. A ring closes in on the tile, and
      the editor comes forward as it arrives — the animation ends when the action happens, not
      before or after.
- [ Y ] Once it is forward, drop the file into the editor's own window. This is the whole point:
      the drag survived the activation.
- [ Y ] Hold over a tile whose application is **not running**. It launches, and the drag is still
      in the air when it appears.
- [ Y ] Spring-load one tile, then move to another and hold. The second one comes forward too.
- [ N (the app is launched every time) ] Return to the tile that already sprang open and hold. It does **not** activate a second
      time — nothing flickers, nothing re-raises.

## It does not fire when it shouldn't

- [ Y ] Drag from one end of the dock to the other at ordinary speed. **Nothing activates.** Do
      this five times; count the accidents. More than zero is a failing default.
- [ Y ] Drag diagonally across a corner of the dock on the way somewhere else. Nothing activates.
- [ Y ] Hold over a tile, then move off it just before the ring closes. Nothing activates, and the
      ring goes with the pointer rather than finishing on an empty cell.
- [ Y ] Hold over a tile and **drop**. The file opens; the application does not also spring open
      behind the drop.
- [ Y ] Hold over a tile and press Escape to cancel the drag. Nothing activates.
- [ Y ] Hold over the shelf **beside** the tiles. Nothing activates — there is no tile there.
- [ ] Hold over a **separator**. Nothing activates.

## The delay itself

- [ Y ] With the default, count silently while holding. The wait should read as deliberate rather
      than as the dock being slow — roughly the length of a decision, not of a pause.
- [ ] Set `springLoadDelay` to 200 ms and repeat the five drags across the dock. Accidental
      triggers should now be easy to provoke; if they are *not*, the default is too slow.
- [ ] Set it to 2000 ms. Holding should now feel like waiting; if it does not, the default is
      too fast.
- [ ] Set it to **0**. Spring-loading is off entirely: no ring, no activation, however long the
      drag rests.
- [ ] Return to the default and confirm the change takes effect without restarting the dock.

## Under the rest of the dock

- [ Y ] Repeat one spring-load with **magnification** enabled. The ring stays on the tile it is
      about as the strip magnifies, and the tile under the pointer is the one that opens.
- [ ] Repeat with the dock on the **left** and on the **right** edge.
- [ ] Repeat at the smallest and largest tile sizes. The ring is proportionate in both.
- [ Y ] After a spring-load, ordinary clicking, dragging and reordering still work — the
      activation did not leave a grab or a drag state behind.
