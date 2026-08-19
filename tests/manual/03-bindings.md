# Manual checklist 03 — binding reachability

`test_dispatch` proves the matrix maps input to commands, and `test_commandrouting` proves
commands reach the backends. Neither can prove the **compositor lets the input through**. KWin
grabs some modifier+click combinations before any client sees them, so a binding can be correct,
routed, tested — and still do nothing on a real desktop.

Everything here runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

This checklist is **interactive** — it needs a human clicking with modifiers held. It cannot be
scripted, which is why it is here and not in `tests/`.

## The specific risk

KWin's own defaults, from `/usr/share/config.kcfg/kwin.kcfg` on the tested machine:

| Key | Default |
|---|---|
| `MouseBindings/CommandAllKey` | `Meta` |
| `MouseBindings/CommandAll1` | `Move` |
| `MouseBindings/CommandAll2` | `Toggle raise and lower` |
| `MouseBindings/CommandAll3` | `Resize` |

So **Meta + any mouse button is claimed compositor-side by default**, which is exactly the
modifier two rows of the matrix use. The open question is whether that grab applies to a
*layer-shell surface*: a dock is not a managed window, so KWin may never consider it a
move/resize target. That is what the Meta rows below are really testing, and it is the reason
this checklist exists.

`Alt` is not in that table, so the Alt rows should be uncontroversial. Verify them anyway —
"should" is how bindings get shipped broken.

## How to observe the result

Watch the dock's own log rather than guessing from screen behaviour, so a binding that arrives
but routes wrongly is distinguishable from one that never arrives at all:

```sh
QT_LOGGING_RULES='frappe.*.debug=true' \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

Three outcomes to tell apart for every row:

1. **Reached and correct** — the intended effect happens.
2. **Reached, wrong effect** — a routing bug. Ours to fix; not a reachability failure.
3. **Never reached** — no effect and no log line, or the compositor visibly did something else
   (a window started moving). This is a swallowed binding and needs an alternative.

## Checklist

Fixture: `pinnedEntries=org.kde.konsole,org.kde.dolphin`, Konsole running with two windows,
Dolphin running with one, Kate installed but not running.

- [ ] **Click → launch.** Click the Kate tile. **Pass:** Kate starts.
- [ ] **Click → activate.** Click the Dolphin tile with its window behind another. **Pass:** the
      window is raised and focused; no second Dolphin starts.
- [ ] **Click → cycle.** Click the Konsole tile twice with its two windows. **Pass:** the second
      click raises the other window, not the same one again.
- [ ] **Meta-click → reveal in file manager.** **Pass:** a file manager window opens with the
      application's `.desktop` file selected. **Watch for:** the dock surface being dragged, or
      nothing happening at all — either means KWin took the event.
- [ ] **Alt-click → activate and hide the app being left.** With Dolphin in front, Alt-click the
      Konsole tile. **Pass:** Konsole comes forward *and* Dolphin's window minimizes.
- [ ] **Meta+Alt-click → activate and hide all others.** **Pass:** the clicked app comes forward
      and every other application's windows minimize. Same watch-for as the Meta row.
- [ ] **Middle-click → new instance.** Middle-click the Konsole tile. **Pass:** a third Konsole
      window appears. **Watch for:** `CommandAll2` (*Toggle raise and lower*) intercepting it.
- [ ] **Right-click → context menu.** **Pass:** `contextMenuRequested` is logged. The menu itself
      is §2.5 and does not exist yet, so the signal is the pass condition.
- [ ] **Press and hold → jump list.** Hold the left button on a tile for about a second.
      **Pass:** `jumpListRequested` is logged, and **no** `tileClicked` is logged for the same
      press — a hold that also fires a click would launch the app underneath the popup.
- [ ] **No modifier leaks.** Shift-click a tile. **Pass:** it launches or activates, the
      documented fallback — it must not be inert.

## Results

**Not run — task 2.4.4 was closed as skipped on 2026-08-17 by user decision.** No row below has
been verified. One attempt was made and produced no usable data: Meta-clicking a tile dragged
the whole nested host window across the real desktop, meaning the *outer* Plasma session ate
the event before the nested compositor saw it. Meta+click is untested, not swallowed.

Anyone picking this up: neutralise the outer session's grab first, or every Meta row will
measure the harness instead of the dock. The outer `kwinrc` carries no `MouseBindings` group,
so it runs the KWin defaults (`CommandAllKey=Meta`, Move / Toggle raise and lower / Resize) —

```sh
kwriteconfig6 --file kwinrc --group MouseBindings --key CommandAll1 Nothing   # and All2, All3
qdbus org.kde.KWin /KWin reconfigure
kwriteconfig6 --file kwinrc --group MouseBindings --key CommandAll1 --delete  # to restore
```

`CommandAllKey` remains `Meta` even then, so pair any negative result with the dock's log to
tell "inner grab" from "outer still filtering".

When it is run, record per row: reached / swallowed, and for anything swallowed the alternative
chosen and why. Findings belong in `docs/decisions/` as well as here, because the settings page
in §7.5 renders the matrix and has to render the *real* one.

## Regressions this checklist has caught

(none yet — this checklist has not been executed)
