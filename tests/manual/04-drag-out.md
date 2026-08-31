# Manual checklist 04 — drag out to remove

`test_dragout` proves unpinning edits one configuration key and nothing else, and
`tst_dragout.qml` proves the affordance appears and disappears with the threshold. What
neither can prove is that the **application is genuinely untouched** — that is a claim about
the system outside the dock's process, and it is the one that matters most here.

Everything runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

## The gesture

- [ N (the tile can be dragged off the dock but it does not render when it leavs the dock)] Drag a pinned tile straight out of the dock. Past roughly one tile's distance from the
      shelf, a **Remove** label appears and the tile dims.
- [ N (there is no label) ] The label is legible and clearly attached to the tile being dragged; it does not run off
      the edge of the screen when the tile is dragged out near a corner.
- [ Y (but there is no label) ] Drag back onto the shelf. The label disappears, the tile rejoins the strip, and releasing
      leaves the dock exactly as it was — including the tile's original position.
- [ Y ] Drag out and release. The tile leaves the dock.
- [ ] Repeat with the dock on the **left** and the **right** edge. "Out" is horizontal there,
      and the threshold is the same distance.
- [ ] Increase the size slider substantially and repeat. The gesture takes proportionally more
      travel and still feels the same — the threshold is a multiple of the tile size.
- [ Y ] Drag a **running but unpinned** application out. No Remove label, and releasing leaves the
      tile alone: it is in the dock because it is running.

## The application is untouched

Before removing, note the entry: `kioclient exec <app>.desktop` or the application launcher.

- [ Y ] After removing, the desktop entry still exists:
      `ls /usr/share/applications/<app>.desktop ~/.local/share/applications/<app>.desktop`
- [ Y ] The application still launches from KRunner / the application launcher.
- [ Y ] Its windows, if it was running when removed, are untouched — nothing was closed or
      minimized.
- [ Y ] `grep -i pinned /tmp/frappe-run/config/frappe-dockrc` shows the entry gone from the pinned
      list, and **no other key changed**.
- [ N (unable to click on Keep on Dock) ] Re-pin it from the context menu of a running instance. It comes back.

## Interaction with everything else

- [ Y ] A plain click still launches; the remove gesture has not eaten ordinary clicks.
- [ Y ] Removing while magnification is enabled behaves the same.
- [ Y ] Removing the last pinned tile leaves an empty but valid dock, not a crash.
