# Manual checklist 05 — minimized windows

`test_minimized` proves the model puts minimized windows in the right region under each
setting. What it cannot prove is that the compositor's idea of "minimized" and ours agree, that
restoring from a dock tile actually brings the window back, or that the setting survives a
restart.

Nested KWin, never the live session:

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

`--no-perm-check` is required, or there is no window list and every item below fails for the
wrong reason.

```ini
[General]
tileSize=64
position=Bottom
minimizeIntoIcon=false
pinnedEntries=org.kde.konsole,org.kde.dolphin
```

Fixture: two Konsole windows and one Dolphin window, so "one window of an app" and "several
windows of an app" are both available, and so a minimized window can be told from its
application's own tile.

## Checklist

### Separate region — `minimizeIntoIcon=false`

- [ ] **Minimizing adds a tile.** Minimize one Konsole window. **Pass:** a new tile appears at
      the end of the dock, after a separator, showing Konsole's icon. The shelf grows by one
      cell pitch plus the separator's width.
- [ ] **The separator appears with the region and not before.** Before any window is minimized
      there is no separator. **Pass:** it appears with the first minimized window and vanishes
      with the last — never a rule with nothing after it.
- [ ] **The application tile is unchanged.** **Pass:** Konsole's own tile stays where it was and
      keeps its running indicator. An application with a minimized window is still running.
- [ ] **Each window gets its own tile.** Minimize the second Konsole window too. **Pass:** two
      tiles in the minimized region, not one.
- [ ] **Tiles are labelled by window, not application.** Hover each. **Pass:** each shows its
      own window title (`cd` somewhere in one first so the two differ). Two tiles reading
      "Konsole" would be useless.
- [ ] **Clicking restores that specific window.** Click the second minimized tile. **Pass:**
      *that* window is restored and focused — not the other one, and not a new instance. Its
      tile disappears; the other minimized tile stays.
- [ ] **Round trip.** Minimize and restore the same window five times. **Pass:** the tile comes
      and goes each time, the dock returns to exactly its previous width, and no tile is left
      behind.
- [ ] **Closing a minimized window.** Minimize a Konsole window, then close it from elsewhere
      (`Alt`+`F4` on it is not possible while minimized — use the context menu's Quit, or
      `kill` its process). **Pass:** the minimized tile disappears without being clicked.
- [ ] **Last window minimized, then quit.** With Konsole's only remaining window minimized, quit
      it. **Pass:** the minimized tile goes and the pinned Konsole tile stays, without a running
      indicator. The separator goes too.
- [ ] **An unpinned application.** Launch Kate (not pinned), minimize it. **Pass:** both its
      application tile and a minimized tile are present. Restore and quit it. **Pass:** both
      disappear.

### Minimize into icon — `minimizeIntoIcon=true`

- [ ] **No region at all.** With the setting on, minimize a window. **Pass:** no new tile, no
      separator, and the dock does not change width. The application's tile keeps its running
      indicator.
- [ ] **Restoring still works from the application tile.** Click the Konsole tile with both its
      windows minimized. **Pass:** a window is restored.

### The toggle itself

- [ ] **Live, with windows already minimized.** With two windows minimized in the separate
      region, switch `minimizeIntoIcon` to `true` in the config file. **Pass:** both minimized
      tiles and the separator disappear immediately, with no restart and no flicker of the
      application tiles. Switch back. **Pass:** both tiles reappear, with the same titles and in
      the same order.
- [ ] **Live, with nothing minimized.** Toggle both ways with no minimized windows. **Pass:**
      nothing visibly changes at all.
- [ ] **Persists across a restart.** Set the toggle, quit the dock, start it again. **Pass:**
      the setting is still in effect and `frappe-dockrc` contains `minimizeIntoIcon=true` under
      `[General]`.

## Results

Not yet run.

## Regressions this checklist has caught

(none yet — this checklist has not been executed)
