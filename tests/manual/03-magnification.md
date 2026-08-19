# Manual checklist 03-magnification — magnification feel

(The number matches the plan phase, not a sequence: `03-bindings.md` is Phase 3's other
checklist and neither supersedes the other.)

`test_magnification` proves the curve is continuous, monotonic and anchored. `tst_magnification`
proves the view draws what the engine published. Neither can prove the result **feels right**,
and no test can: the parameters are not recovered constants, they are choices, and the only
instrument that reads them is a hand on a pointing device.

This checklist is what §3.13 freezes the parameters against. Run it before writing the decision
record, not after.

Everything here runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)

# A scratch config, so tuning does not disturb the real one.
mkdir -p /tmp/frappe-tune/config
XDG_CONFIG_HOME=/tmp/frappe-tune/config FRAPPE_TUNING=1 \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

`FRAPPE_TUNING=1` opens the tuning harness alongside the dock. Its sliders mutate the running
dock, and **Save** writes them to the config file — so what is dialled in here is what ships.
Development builds carry it; a Release build does not (`-DFRAPPE_TUNING_HARNESS=OFF` is implied
by `CMAKE_BUILD_TYPE=Release`).

## What is actually being judged

Three things, and it helps to judge them one at a time rather than forming a general impression:

1. **Tracking.** Does the peak follow the pointer, or lag behind it? Lag is the animation
   duration being too long. Rubber-banding is `SmoothedAnimation` being asked to cross too much
   distance too slowly.
2. **The shape of the bulge.** How many tiles are involved, and how abruptly does the effect
   end? Falloff radius sets the count; the curve exponent sets whether the edge of the effect
   is a soft shoulder or a visible seam.
3. **Aiming.** Can you land on the tile you meant to? This is the one users notice without
   being able to name. It fails when the peak is too high relative to the falloff — the tile
   you are aiming at shoves itself out from under the pointer as you approach.

Item 3 has a hard invariant behind it (`thePointedTileStaysUnderThePointer`), so it cannot fail
outright. It can still feel bad while remaining correct, which is what you are here to detect.

## Tile counts

Set `pinnedEntries` in the scratch config, or drag entries in. The point of the range is that
compression and magnification interact: a dense dock magnifies from a smaller resting size, and
the strip has less room to grow into before it hits the shelf's budget.

| # | Tiles | Watch for | Pass |
|---|---|---|---|
| 1 | 3 | Sparse. The effect has almost nothing to fall off onto — the bulge should still look like a bulge and not like one tile inflating on its own. | ☐ |
| 2 | 10 | The ordinary case. Transitions smooth, no stepping, no visible seam where the falloff ends. | ☐ |
| 3 | 25 | Compression is now active: resting tiles are below `tileSize`. Magnification should still reach the configured peak. | ☐ |
| 4 | 40 | Dense. Resting size at or near the floor. The shelf must not overflow the screen, and the strip must not tear away from it. | ☐ |

For 4, confirm the floor is doing its job: tiles stop shrinking and the shelf overflows its
budget rather than the tiles vanishing. That is the design correction, and it is meant to be
visible.

## Tile sizes

| # | `tileSize` | Watch for | Pass |
|---|---|---|---|
| 5 | 24 (minimum) | Everything proportional — gap, padding, radius, indicator. The peak is 2 x 24 = 48, which is small; check the effect is still legible. | ☐ |
| 6 | 128 (maximum) | Wide spacing, large peak. The strip is long; check it compresses rather than running off the screen, and that the shelf's growth still tracks the pointer. | ☐ |

A dock at 24 must be a uniform scale of a dock at 128. If it is not, a literal has been left
somewhere and `proportionsFollowTileSize` did not catch it.

## Orientation

| # | Position | Watch for | Pass |
|---|---|---|---|
| 7 | Left | Magnification along Y. Tiles centred on X as they grow. Shelf grows and moves vertically. | ☐ |
| 8 | Right | The same, mirrored. Nothing should feel different from Left. | ☐ |

The axis swap is one `horizontal ?` per binding, which is exactly the shape of bug that survives
unit tests: verify the *cross* axis too, i.e. that growing tiles stay centred on the shelf rather
than drifting toward one rim.

## Input devices

The engine sees a stream of pointer positions and does not know where they came from. What
differs is the sampling.

| # | Device | Watch for | Pass |
|---|---|---|---|
| 9 | Trackpad | Dense, smooth motion. The peak should track continuously with no stutter. This is the case `SmoothedAnimation` was chosen for — a `NumberAnimation` restarting per event would show here first. | ☐ |
| 10 | Mouse | Coarser steps, especially at low DPI. Motion between samples must be interpolated, not stepped: the dock should not visibly jump between tiles. | ☐ |

Also worth trying with the mouse: move fast, stop dead. The layout should settle without
overshoot, and without a visible catch-up slide.

## Entering and leaving

| # | Case | Watch for | Pass |
|---|---|---|---|
| 11 | Enter slowly from the side | Growth begins before the pointer is over the first tile — the falloff reaches past the shelf's edge. | ☐ |
| 12 | Enter fast from below | No pop. The first frame should not show a fully magnified dock. | ☐ |
| 13 | Leave in any direction | Returns to rest on the same curve it grew on. No tile left slightly enlarged. | ☐ |
| 14 | Leave by moving *along* the shelf and off the end | The shelf shrinks back and re-centres without the strip sliding inside it. | ☐ |
| 15 | Start the dock with the pointer already over it | Correct layout on the first frame, and no assembly animation. | ☐ |

15 is a regression check: the dock used to animate every tile in from its placeholder at startup.

## Peak size

The peak is the parameter with a structural consequence rather than only a felt one: the surface
has to be thick enough to draw the largest tile it can produce, and tiles have to grow *away*
from the screen edge rather than across it.

| # | `magnificationFactor` | Watch for | Pass |
|---|---|---|---|
| 19 | 3.0 (maximum) | **No clipping.** Icons must not be cut off at the top (or the outer side, when side-mounted). | ☐ |
| 20 | 3.0, side-mounted | The same on the left and right edges, where the growth direction differs. | ☐ |
| 21 | 3.0, then 1.0, then 3.0 | The surface resizes with the setting. No stale thickness, no icons clipped after a change. | ☐ |
| 22 | 3.0 | Windows are **not** pushed up by the headroom: the reserved area is the shelf, not the surface. Maximise a window and check its bottom edge sits just above the shelf. | ☐ |
| 23 | 3.0 | Clicks in the empty space above the shelf reach the window behind, rather than being swallowed. | ☐ |

Rows 19–21 are the regression: they failed before the fix recorded in 3.5.6's notes. 22 and 23
are the two things that fix could plausibly have broken.

## Animation speed

| # | `animationSpeed` | Watch for | Pass |
|---|---|---|---|
| 16 | 0 | No animation at all. Layout still correct, just instant. This is the reduced-motion path. | ☐ |
| 17 | 400 | Fastest. Should read as instant-but-not-jarring rather than broken. | ☐ |
| 18 | 25 | Slowest. Useful for *seeing* the transitions you are judging in the rows above. Not a shipping value. | ☐ |

## Pass criteria

The plan's bar is **"it feels right at both extremes"** — 3 tiles and 40, 24 pt and 128 pt.
Concretely, all of:

- No row above marked failed.
- Aiming (item 3) is comfortable at every tile count without conscious correction.
- Nothing about the motion draws attention to itself. The magnification should be noticed only
  when looked for.

## When it passes

Record the chosen parameters and the reasoning in `docs/decisions/`, and set them as the
defaults in `frappeconfig.kcfg`. That is task 3.13, and it is the point of this checklist:
these values are confidence **C** — they are choices made against a feel, not measurements, and
the record needs to say so plainly enough that nobody later mistakes them for recovered
constants.

Note which rows drove which parameter. "40 tiles at 24 pt needed a shorter radius" is worth more
to whoever revisits this than the final number alone.
