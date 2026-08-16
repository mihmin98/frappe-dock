# Manual checklist 02 — running indicators

Whether the dot tracks real windows cannot be unit tested: `tst_indicator.qml` proves the
component reacts to `isRunning`, and `test_tilemodel_merge` proves the model sets it, but only a
compositor closes the loop from "an application started" to "a dot appeared". Everything here
runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

**`--no-perm-check` is not optional here.** Without it the window-management model returns zero
rows and emits no signals — silently, with no error — so every check below fails identically and
for the wrong reason. See `docs/decisions/2026-08-16-libtaskmanager-api-surface.md`. If the very
first check fails, confirm the flag before debugging anything else.

Settings under test, in `/tmp/frappe-run/config/frappe-dockrc`, so the run cannot touch your real
ones:

```ini
[General]
tileSize=64
position=Bottom
showRunningIndicators=true
pinnedEntries=org.kde.konsole,org.kde.dolphin
```

Konsole is pinned and Kate deliberately is not: the pinned and unpinned close behaviours differ,
and one app cannot demonstrate both.

## How the checks are made

Dots are small — 0.085 S, which is 5 px at S = 64 — so eyeballing is not a pass for the position
checks. Two methods:

**Presence/absence** is safe to eyeball: a dot is there or it is not.

**Position** is measured off a screenshot. Capture with `--shot`, then find the painted pixels
below (or beside) the artwork:

```sh
./tools/run-nested.sh --no-perm-check --shot /tmp/dock.png ./build/bin/frappe-dock
python3 -c "
from PIL import Image
im=Image.open('/tmp/dock.png').convert('RGBA'); px=im.load(); w,h=im.size
pts=[(x,y) for x in range(w) for y in range(h) if px[x,y][3]>10]
print('painted x',(min(p[0] for p in pts),max(p[0] for p in pts)))
print('painted y',(min(p[1] for p in pts),max(p[1] for p in pts)))"
```

Expected values at S = 64, from Part 0:

| Quantity | Rule | At S = 64 |
|---|---|---|
| Dot diameter | `0.085 S` | 5.4 px |
| Dot centre, clear of the artwork's edge | `0.21 S` | 13.4 px |

Tolerance is 1 px on the diameter and 2 px on the offset — these are ratios of a measurement with
a ~3 % error bar, not recovered constants.

To watch the model rather than the pixels, run with the category enabled:

```sh
QT_LOGGING_RULES='frappe.tasks.info=true' ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

Any app id printed there is one whose tile will be named after its raw app id with a placeholder
icon. That is expected for interpreters and wrappers; note what appears rather than fixing it
per-application.

## Checklist

- [ ] **Indicator appears when an app launches.** With the dock up and nothing running, no tile
      carries a dot. Launch Konsole from inside the nested session. **Pass:** a dot appears under
      the Konsole tile within one frame of the window mapping, without any other tile changing.
- [ ] **Indicator appears for an unpinned app, along with its tile.** Launch Kate, which is not
      in `pinnedEntries`. **Pass:** a new tile appears after the two pinned ones, carrying a dot.
      The pinned tiles do not move.
- [ ] **Indicator disappears when the last window closes — unpinned.** Close Kate. **Pass:** the
      Kate tile is gone entirely, dot and all, and the shelf shrinks by one cell pitch
      (`S + S/3` = 85 px at S = 64). Konsole's tile does not shift.
- [ ] **Indicator disappears but the tile stays — pinned.** Close Konsole. **Pass:** the Konsole
      tile remains in place at the same position, and only its dot disappears. The shelf does not
      change size.
- [ ] **Several windows of one app.** Open three Konsole windows. **Pass:** exactly one Konsole
      tile with exactly one dot — window counting is off by default. Close two of the three:
      still one dot. Close the third: no dot, tile stays.
- [ ] **Indicator position, bottom dock.** `position=Bottom`, Konsole running, measured from a
      screenshot. **Pass:** the dot is centred horizontally on the artwork, and its centre sits
      13–14 px *below* the artwork's bottom edge — on the outer side, between the art and the
      screen border — with a diameter of 5–6 px.
- [ ] **Indicator position, left dock.** `position=Left`. **Pass:** the dot is centred vertically
      on the artwork and its centre sits 13–14 px to the *right* of the artwork's right edge —
      the inner side, facing the desktop, not the screen border.
- [ ] **Indicator position, right dock.** `position=Right`. **Pass:** mirrored — 13–14 px to the
      *left* of the artwork's left edge, again facing the desktop.
- [ ] **Config toggle hides and shows it live.** With Konsole running, edit
      `showRunningIndicators=false` into the running instance's config file and let KConfig pick
      it up (or use the settings UI once §7.4 exists). **Pass:** every dot disappears without the
      dock restarting, tiles keep their positions and sizes, and setting it back to `true`
      restores them. A dock that has to be restarted is a fail.
- [ ] **Dots scale with tile size.** Restart at `tileSize=32` and again at `tileSize=96`.
      **Pass:** dot diameter measures ~2.7 px and ~8.2 px, and the offset ~6.7 px and ~20 px —
      a uniform scale, no fixed pixel sizes anywhere.

## Regressions this checklist has caught

(none yet — this checklist has not been executed)
