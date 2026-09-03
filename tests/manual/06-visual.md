# Manual checklist 06 — visual system

Blur, the shelf's corner, and legibility over real content cannot be unit tested: they are
properties of what the compositor puts on screen, not of what the dock computes.
`test_palette`, `test_iconpipeline` and `test_appearancemodes` cover everything that can be
decided headless — this file covers only what is left. Everything here runs in **nested KWin**,
never against the live session.

## Setup

```sh
cmake --build build -j$(nproc)
mkdir -p /tmp/frappe-run/config
```

`/tmp/frappe-run/config/frappe-dockrc`, so the run cannot touch your real settings:

```ini
[General]
tileSize=64
position=Bottom
magnificationEnabled=true
appearanceMode=Default
pinnedEntries=org.kde.konsole,org.kde.dolphin,org.kde.kate,systemsettings,firefox
```

The pin list needs **both kinds of icon** or half this file cannot be checked: a full-bleed
themed icon that the pipeline must pass through untouched, and a flat or symbolic one it must
mask. Substitute whatever is installed; check with
`ls ~/.local/share/applications /usr/share/applications | head -40`.

### The backdrop, and why there has to be one

The dock reserves a zone, so nothing is behind it and **a blur check against an empty nested
desktop passes vacuously** — this is the mistake the 0.5 spike made and had to redo. There is
no wallpaper in a nested session either, so the three-wallpaper checks need something to stand
in for one.

Both problems have the same answer: a **fullscreen** window. Fullscreen ignores the reserved
zone, so it runs under the dock; the layer surface sits on `LayerTop`, above it.

`/tmp/frappe-run/backdrop.qml`:

```qml
import QtQuick

Window {
    visible: true
    visibility: Window.FullScreen
    color: mode === "busy" ? "black" : mode

    // "white", "black", or "busy". Busy is 100x90 saturated blocks: the blur
    // check needs a boundary with a known, large channel step across it, and a
    // photograph does not have one.
    property string mode: Qt.application.arguments[1] || "busy"

    Grid {
        visible: parent.mode === "busy"
        columns: 16
        Repeater {
            model: 16 * 9
            Rectangle {
                width: 100; height: 100
                color: Qt.hsva((index % 16) / 16, 1, (index % 2) ? 1 : 0.25, 1)
            }
        }
    }
}
```

`/tmp/frappe-run/session.sh` — backdrop first, dock second, so the dock is on top:

```sh
#!/bin/sh
export XDG_CONFIG_HOME=/tmp/frappe-run/config
export QT_FORCE_STDERR_LOGGING=1 QT_ASSUME_STDERR_HAS_CONSOLE=1
qml6 /tmp/frappe-run/backdrop.qml -- "${BACKDROP:-busy}" &
sleep 1
FRAPPE_TUNING=1 ./build/bin/frappe-dock
```

```sh
chmod +x /tmp/frappe-run/session.sh
./tools/run-nested.sh --no-perm-check /tmp/frappe-run/session.sh
```

`--no-perm-check` so the window-management model returns rows; without it the dock is pinned
tiles only and nothing about running applications can be judged. `FRAPPE_TUNING=1` opens the
tuning harness, which is where the size slider and the icon-treatment selector live — until
§7.3's Appearance page exists it is the only way to change either without a restart, and
several checks below need exactly that.

## How the checks are made

### Blur — measured, never eyeballed

Eyeballing gave the **wrong answer** in the 0.5 spike: sampling block centres makes an
unblurred translucent strip look like a smooth gradient. Sample a horizontal run of pixels
across a known block boundary, once above the dock and once inside it, and take the largest
single-pixel channel step.

```sh
BACKDROP=busy ./tools/run-nested.sh --no-perm-check --shot /tmp/dock.png /tmp/frappe-run/session.sh
python3 -c "
from PIL import Image
im=Image.open('/tmp/dock.png').convert('RGB'); px=im.load(); w,h=im.size
def step(y):
    return max(max(abs(px[x+1,y][c]-px[x,y][c]) for c in range(3)) for x in range(w-1))
print('above dock ', step(h//2))
print('inside dock', step(h-40))"
```

| Reading | Means |
|---|---|
| above ≫ 20, inside ≤ 5 | blurred |
| above ≫ 20, inside ≈ above | not blurred |
| above ≤ 5 | the backdrop is wrong, not the dock — fix that first |

The spike's figures were 66 above and 1 inside when blurred, 26 inside when not.

### Geometry — measured off the alpha channel

```sh
python3 -c "
from PIL import Image
im=Image.open('/tmp/dock.png').convert('RGBA'); px=im.load(); w,h=im.size
ys=[y for y in range(h) for x in range(0,w,7) if px[x,y][3]>10]
print('painted y', (min(ys), max(ys)), 'screen', (w,h))"
```

With a fullscreen backdrop everything is opaque, so measure the **shelf** from the paint
instead: `--shot` with no backdrop for the geometry rows, and with one for the blur and
legibility rows.

---

## Checklist

Each row is a pass only if the stated criterion is met. "Looks fine" is not a criterion.

### Blur

- [ ] **1. Blur with the effect on.** `kwin-effects-better-blur-dx` enabled in the outer
      session's `kwinrc` (it is what the nested KWin reads).
      **Pass:** inside-dock step ≤ 5 with the above-dock step > 20, by the measurement above.

- [ ] **2. Blur with the effect off.** Disable the effect
      (`kwriteconfig6 --file /tmp/frappe-run/config/kwinrc --group Plugins --key better-blur-dxEnabled false`
      and run with `XDG_CONFIG_HOME` covering the nested KWin too).
      **Pass:** the shelf is still drawn, still translucent, still rounded, and the dock does
      not warn or misbehave. Flat translucency is the documented fallback and needs no code —
      this row confirms it needs none.

- [ ] **3. Blur follows the shelf under magnification.** Magnification on, pointer resting mid-strip.
      **Pass:** no unblurred band along the grown edges of the shelf. The blur region is
      re-sent on every shelf geometry change; a dry band means the report stopped arriving.

- [ ] **4. Blur does not leak outside the shelf.** Pointer away from the dock.
      **Pass:** the blurred area ends where the shelf's paint ends. A blurred strip running the
      full width of the screen means the region was lost and the whole surface is being blurred
      — the surface spans the output, so this is the failure mode to watch for.

### Shelf geometry

- [ ] **5. Corner radius tracks the size slider.** Drag the harness's *Tile size* slider across
      its **full range, 24 → 128**.
      **Pass:** the corner visibly opens up as the slider rises, continuously. A radius that
      stays put means either a literal in the QML or a KWin effect drawing it. At S = 24 the
      radius is 11 pt and at S = 128 it is 60 pt — a 5× change, unmissable.

- [ ] **6. No doubled rounding.** At S = 24 and again at S = 128, look at a shelf corner over
      the busy backdrop.
      **Pass:** one corner, not two. `better-blur-dx` applies its own `CornerRadius` (26 in the
      stock config) to the region it blurs; ours scales. They agree near S = 55 and diverge at
      both ends, showing as blur overhanging or falling short of the painted corner. If you see
      it, set `CornerRadius=0` under `[Effect-better-blur-dx]` — we hand over an already-rounded
      region. This is a **known interaction, recorded in
      `docs/decisions/2026-09-03-shelf-radius-ownership.md`**, not a regression; note which it
      is rather than just failing the row.

- [ ] **7. The 1 pt rim stroke survives.** At S = 24 and S = 128, over all three backdrops.
      **Pass:** a continuous hairline around the **full perimeter** at both sizes — it does not
      scale, so at S = 128 it must still be 1 pt and not have grown, and at S = 24 it must not
      have vanished. Brightest along the top edge is expected, not required.

- [ ] **8. The shelf floats clear of the screen edge.** Any size.
      **Pass:** a visible gap of `tileSize / 9` between the shelf's outer edge and the screen
      edge — 7 pt at S = 64. A shelf sitting flush is the usual symptom of the gap being lost to
      the reserved zone, which is why this is here and not only in `tst_tile.qml`.

### Colour

- [ ] **9. Legibility over three backdrops.** Run three times, `BACKDROP=white`, `black`, `busy`.
      **Pass:** in all three — shelf distinguishable from what is behind it; rim visible; the
      running-indicator dots visible; any drop-refusal text readable. The dock takes its colours
      from the scheme's Complementary set precisely so this holds without per-wallpaper tuning.

- [ ] **10. Colour scheme change applies live.** With the dock running, on the same session bus:
      `XDG_CONFIG_HOME=/tmp/frappe-run/config plasma-apply-colorscheme BreezeLight`, then
      `BreezeDark`.
      **Pass:** shelf, rim, text and indicators all move, without restarting the dock. The
      writer has to notify over D-Bus for the watcher to hear it, which
      `plasma-apply-colorscheme` does and a hand-edit of the file does **not** — if nothing
      happens, check you did not just edit `kdeglobals` in a text editor.

### Icons

- [ ] **11. Conforming icons are unmodified.** Compare a full-bleed themed icon in the dock
      against the same icon elsewhere.
      **Pass:** identical artwork. No plate behind it, no inset, no recolouring.

- [ ] **12. Non-conforming icons are masked, with colour kept.** Find a flat or symbolic pin.
      **Pass:** it sits on a rounded plate that fills its cell, the plate carries **that icon's
      own colour**, and the artwork on top is unrecoloured. A grey plate is a failure — that is
      design correction #1, and `maskingPreservesColour()` is its unit-test half.

- [ ] **13. Appearance mode applies live.** Cycle the harness's *Icon treatment* selector
      through Default → Dark → Tinted → Default.
      **Pass:** the tiles change **immediately**, with no restart, every time. Stale artwork
      here means the cache token stopped moving: the pipeline's own cache is dropped on every
      mode change, but QML caches by URL and will not ask again unless the URL changes.

- [ ] **14. Every mode still identifies an icon.** In each of the three modes, with 5+ pins.
      **Pass:** you can tell the tiles apart without reading labels. Default and Dark keep
      colour; Tinted spends it and must keep shape. A mode where the tiles are interchangeable
      is a hard failure, not a taste question — see §6.3 and `test_appearancemodes.cpp`.

### Performance

- [ ] **15. No stutter while magnifying.** Sweep the pointer along the strip, at S = 64 and
      S = 128, over the busy backdrop.
      **Pass:** motion stays smooth. The blur region is re-sent whenever the shelf moves or
      grows, which under magnification is every frame, and every *rectangle* of that region is
      a separate Wayland request. Two things keep that affordable and this row is what checks
      they still do: the corners are a bounded staircase rather than a scanline ellipse
      (`blurRegion()`, ~17 rectangles at S = 128 against 86), and the shelf's four separate
      geometry reports are coalesced into one update per turn of the event loop.

- [x] **16. Drag a tile out of the dock and move it around.** *(passed 2026-09-03, after the
      reorder fix)* Past the removal threshold, then
      around the screen for several seconds, at S = 128 over the busy backdrop.
      **Pass:** the dock keeps redrawing and the Remove affordance keeps following the pointer.
      **Regression, 2026-09-03:** this froze the dock. The blur region was being re-sent about
      350 Wayland requests' worth per frame. `test_blurregion.cpp` guards the region's cost
      headlessly; only this row shows whether the dock survives the gesture.

      **The cause was found on 2026-09-03 and it was neither of the blur defects below:**
      `moveTile()` accepted moves that `rebuild()` immediately undid, so the drag re-asked on
      every pointer event and each round did a disk write and a full surface reconfigure. See
      `.tasks/04-direct-manipulation/01-reordering.md`. Also drag a tile **over** the strip,
      across the boundary between pinned tiles and running unpinned ones, and across the file
      separator: the tile should simply refuse to cross, and the dock should stay responsive.

      Two further defects were fixed off the back of this gesture, either of which could have
      contributed:
      the blur-region cost above, and a `restingOrigin` binding loop that fired on every
      pointer move (`DockGeometry` published the resting layout on the same signal as the
      magnified one). **Watch the log** — `Binding loop detected` must not appear at all.

      If it freezes again, disable `better-blur-dx` and repeat. Still frozen means the cause is
      neither, and the next suspect is the reorder path: `moveTile()` writes config, and the
      resulting `changed()` rebuilds the model and reconfigures every surface, mid-gesture.

## Recording a run

Note the date, the KWin version (`kwin_wayland --version`), whether `better-blur-dx` was
enabled, and any row that failed with its measured numbers. Rows 6 and 10 have known external
dependencies — say which applied rather than only that the row passed.
