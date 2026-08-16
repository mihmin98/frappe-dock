# Manual checklist 01 — layer surface

Layer-shell anchoring, space reservation and output changes cannot be unit tested: they need a
real compositor. Everything here runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config ./tools/run-nested.sh ./build/bin/frappe-dock
```

Put the settings under test in `/tmp/frappe-run/config/frappe-dockrc` so the run cannot touch
your real ones:

```ini
[General]
tileSize=64
position=Bottom
pinnedEntries=org.kde.konsole,org.kde.dolphin,systemsettings,org.kde.kate,firefox
```

## How the numeric checks are made

Eyeballing a screenshot is not a pass. Two harnesses give numbers:

**Geometry** — capture with `--shot` and measure the painted region's alpha extent:

```sh
./tools/run-nested.sh --shot /tmp/dock.png ./build/bin/frappe-dock
python3 -c "
from PIL import Image
im=Image.open('/tmp/dock.png').convert('RGBA'); px=im.load(); w,h=im.size
xs=[x for x in range(w) for y in range(0,h,7) if px[x,y][3]>10]
ys=[y for y in range(h) for x in range(0,w,7) if px[x,y][3]>10]
print('x',(min(xs),max(xs)),'y',(min(ys),max(ys)),'screen',(w,h))"
```

**Reservation** — a maximised window reports its own size, so the reserved strip is arithmetic
rather than judgement. `/tmp/probe.qml`:

```qml
import QtQuick
Window {
    visible: true
    visibility: Window.Maximized
    Timer { interval: 2500; running: true
            onTriggered: { console.warn("WIN " + parent.width + "x" + parent.height); Qt.quit() } }
}
```

launched after the dock, in one script passed to `run-nested.sh`:

```sh
#!/bin/sh
QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1 ./build/bin/frappe-dock &
sleep 2
QT_FORCE_STDERR_LOGGING=1 qml6 /tmp/probe.qml
```

`QT_FORCE_STDERR_LOGGING=1` is not optional: without it the child's output is discarded and the
probe appears to print nothing.

**Note on the probe's own overhead.** With a second window present, KWin gives the probe a
34 px decoration on a 1600x900 nested output. Subtract it before comparing heights; widths are
unaffected, which is why the Left/Right checks are the cleaner measurement of the two.

## Expected geometry

At `tileSize` = S, from the proportion model (plan.md Part 0):

| Quantity | Rule | At S = 64 |
|---|---|---|
| Shelf thickness | `S + 2(S/3)` | 107 |
| Gap, shelf → screen edge | `S/9` | 7 |
| Reserved strip | thickness + gap | 114 |
| Shelf length, 5 tiles | `5S + 6(S/3)` | 448 |

The compositor **adds the layer-shell margin to the exclusive zone**, so the zone the dock sets
is the thickness alone and the reserved strip comes out as thickness + gap. Setting the zone to
thickness + gap reserves the gap twice and leaves a dead strip above the dock.

## Checklist

- [x] **Anchors to bottom edge.** Shelf spans y 788–892 on a 900-high output: 105 px painted
      against a 107 px surface, bottom edge 8 px clear of the screen edge against a 7 px margin
      plus the 1 pt rim. Horizontally centred, 448 px wide for five tiles — exactly the rule.
- [ ] **Anchors to top edge.** **Not applicable.** `position` offers Bottom, Left and Right
      only (§1.2 schema); there is no top dock, matching the reference behaviour. Recorded
      here rather than silently dropped, because the plan's generic wording lists it.
- [x] **Anchors to left edge.** Shelf at x 8–113, y 226–673: 105 px thick, 8 px clear of the
      left edge, 448 px long and vertically centred (midpoint 449.5 against a screen midpoint
      of 450).
- [x] **Anchors to right edge.** Shelf at x 1487–1592 on a 1600-wide output: 7 px clear of the
      right edge, same thickness and length, same centring.
- [x] **Reserves correct space.** Maximised window came back 1486 wide against a 1600 output
      with a left dock — exactly 114 reserved, the thickness plus the gap. Bottom dock: 752
      high against 900, i.e. 114 reserved once the probe's 34 px decoration is subtracted.
      Cross-checked at `tileSize` 48 and 96, both matching thickness + gap to the pixel.
- [ ] **Survives resolution change.** **Not run.** Nested KWin's output size is fixed at
      launch, so this needs either `--virtual` output reconfiguration or a real display change.
      Partial evidence: the dock anchors and centres correctly at 1600x900 and at 800x600
      chosen at launch, and `OutputProvider` reconciles on `QScreen::geometryChanged`. Full
      verification belongs with the multi-output work in 8.4.

## Regressions this checklist has caught

- **A second surface on an unnamed output.** Qt keeps a placeholder `QScreen` with an empty
  name and briefly reports it alongside real outputs while they are being reconfigured. Taking
  it for an output produced a second, stacked dock surface. `OutputProvider::outputs()` now
  skips unnamed screens.
- **The exclusive zone double-counting the screen gap**, as described above.
