# Manual checklist 05 — stacks

`test_stackmodel`, `test_sorting`, `test_viewmodepersist` and `test_stackpositioning` prove the
model: contents match a directory, live updates arrive, each sort order is correct and breaks
ties by name, the mode persists per folder, and the placement is right for all four edges,
including `noPositionalJumpOnPointerMove()`. `tst_gridview.qml`, `tst_fanview.qml`,
`tst_listview.qml` and `tst_stackpopup.qml` prove the views are wired to it.

None of that runs against a compositor, and four things about stacks only exist there:

- The stack's rectangle joins the surface's **input region** while it is open
  (`setMask(shelfRect | stackRect)`). Off by anything, the stack is drawn and dead to the
  pointer, or clicks over empty desktop land in it. No headless test can see this.
- Whether a few hundred entries are actually *usable* — scrolling, hit rate, whether the labels
  can be read — rather than merely present in a model.
- Whether the fan and the list read as what they are at real sizes on a real screen.
- The §11 regression, end to end: `noPositionalJumpOnPointerMove()` asserts the placement
  function is constant under the pointer, but only a live pointer over a live magnified strip
  shows the *drawn* stack not moving.

Nested KWin, never the live session:

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

`$XDG_CONFIG_HOME/frappe-dockrc`:

```ini
[General]
tileSize=64
position=Bottom
magnificationEnabled=false
pinnedEntries=org.kde.konsole,org.kde.dolphin
fileEntries=/tmp/frappe-fixture/mixed,/tmp/frappe-fixture/few,/usr/share/applications
```

Fixtures, created before starting the dock:

```sh
mkdir -p /tmp/frappe-fixture/mixed/nested/deeper /tmp/frappe-fixture/few /tmp/frappe-fixture/empty
cd /tmp/frappe-fixture/mixed
for i in $(seq -w 1 40); do : > "file-$i.txt"; done
cp /usr/share/icons/hicolor/48x48/apps/*.png . 2>/dev/null || : > image.png
: > nested/inside.txt
: > nested/deeper/bottom.txt
touch -d '2020-01-01' file-01.txt          # a clear oldest, for the date orders
: > /tmp/frappe-fixture/few/a.txt
: > /tmp/frappe-fixture/few/b.txt
: > /tmp/frappe-fixture/few/c.txt
```

Three folder tiles: **mixed** (40+ entries, drills two levels deep), **few** (three entries, so
the fan is a fan rather than a cap), and **applications** (~200 entries, the size that decides
whether Grid was worth building).

Delete `/tmp/frappe-fixture` and the `[Stacks]` group of `frappe-dockrc` between runs — a stored
view mode from a previous session makes half of this checklist start in the wrong state.

## Checklist

### Opening and closing

- [ ] **A folder tile opens a stack, it does not launch anything.** Click the *mixed* tile.
      **Pass:** a grid of the folder's contents appears above the shelf. No file manager window
      opens.
- [ ] **The stack is where the tile is.** **Pass:** it is centred on the *mixed* tile along the
      dock's axis, and clear of the shelf across it — the gap between shelf and stack is about
      one inter-icon gap (S/3 ≈ 21 pt at `tileSize=64`), not zero and not a wide band.
- [ ] **It takes clicks.** Click a file's icon in the open stack. **Pass:** the file opens. This
      is the input-region check: if the region were not extended, the click would fall through to
      whatever is behind the stack.
- [ ] **Clicking the same tile again closes it.** **Pass:** the stack disappears; it does not
      reopen or flicker.
- [ ] **Clicking away closes it.** Open a stack and click bare desktop well away from it.
      **Pass:** the stack closes, and the click is spent on the dismissal — it does not also
      reach the desktop or whatever window is under it.
- [ ] **A near miss is not a dismissal.** Click the stack's own padding — its corner, or the gap
      between two icons. **Pass:** nothing happens and the stack stays open. Missing an icon by
      two pixels must not close it.
- [ ] **Clicking another tile still works.** With a stack open, click a different application's
      tile. **Pass:** the application activates. The dismissal backdrop sits behind the shelf and
      does not swallow it.
- [ ] **Escape closes it.** **Pass:** the stack closes. The dock's layer surface takes keyboard
      focus on demand, so if Escape does nothing here, note whether the dock had focus — that
      distinguishes a missing shortcut from a surface that never received the key.
- [ ] **Escape with nothing open.** **Pass:** nothing happens, and Escape still reaches whatever
      application has focus. The dock does not hold the key hostage.
- [ ] **Clicks stop landing in it once closed.** With the stack closed, click the empty desktop
      where the stack was. **Pass:** the desktop takes the click. Nothing invisible eats it and
      nothing opens.
- [ ] **Opening a second stack closes the first.** With *mixed* open, click *few*. **Pass:**
      exactly one stack is on screen, showing *few*.
- [ ] **Reopening starts at the folder's root.** Open *mixed*, drill into `nested`, close, reopen.
      **Pass:** it shows the root of *mixed*, not `nested`.

### At each dock edge

Set `position` and repeat. Every edge gets its own line because the placement arithmetic is
mirrored per edge and a mirror is where a sign error hides.

- [ ] **Bottom.** **Pass:** the stack sits above the shelf, centred on the tile, fully on screen.
- [ ] **Left.** **Pass:** it sits to the right of the shelf, centred on the tile vertically.
- [ ] **Right.** **Pass:** it sits to the left of the shelf, and the fan leans left rather than
      off the screen edge.
- [ ] **Top** is not reachable from the `position` key by design — three values, not four. Nothing
      to check; noted so its absence is not read as an omission.
- [ ] **Corner clamping.** With the dock on Bottom, drag a folder tile to the far left end of the
      dock (or reorder it there) and open it. **Pass:** the stack shifts inwards to stay fully on
      screen rather than being clipped at the screen edge, and it still visually belongs to that
      tile.
- [ ] **The stack follows a reflow.** With a stack open, launch an unpinned application so the
      dock grows and the tiles shift. **Pass:** the stack moves with its tile and stays clickable
      — the input region followed it. Click an entry to confirm.

### Grid view

Default mode; no menu step needed.

- [ ] **Contents match.** Open *mixed*. **Pass:** every file in the directory is present, with
      folder icons for folders and type icons for files. `ls /tmp/frappe-fixture/mixed | wc -l`
      matches what is shown.
- [ ] **It scrolls rather than growing.** **Pass:** the grid is about four rows tall and scrolls;
      it does not stretch to 40 entries or off the top of the screen. Scroll with the wheel and
      by dragging. Both reach the last row.
- [ ] **Drill down.** Click `nested`. **Pass:** the grid shows `nested`'s contents, the breadcrumb
      gains a crumb, and the back arrow becomes fully opaque. The stack does not move to a
      different place on screen while doing so.
- [ ] **Drill down again.** Click `deeper`. **Pass:** two crumbs; `bottom.txt` is shown.
- [ ] **Drill up.** Click the back arrow. **Pass:** one level up, one crumb fewer. Click it again:
      back at the root, and the arrow is dimmed and inert.
- [ ] **The breadcrumb navigates.** From `deeper`, click the first crumb. **Pass:** straight back
      to the root in one step. Clicking the *last* crumb — where you already are — does nothing.
- [ ] **Scroll position resets on navigation.** Scroll to the bottom of *mixed*, enter `nested`,
      then go back up. **Pass:** each new listing starts at the top; the view is never parked
      past the end of a shorter folder.
- [ ] **A file opens, a folder enters.** Click `file-01.txt`. **Pass:** it opens in its default
      application. The stack does not try to enter it.
- [ ] **Live update.** With *mixed* open, `touch /tmp/frappe-fixture/mixed/zz-new.txt` from a
      terminal. **Pass:** the entry appears without reopening the stack. Then `rm` it: it goes.
- [ ] **Empty folder.** Add `/tmp/frappe-fixture/empty` to `fileEntries` and open it. **Pass:** a
      placeholder saying the folder is empty — not a blank rectangle, and not a crash.
- [ ] **Deleted folder.** With *few* open, `rm -rf /tmp/frappe-fixture/few`. **Pass:** the stack
      empties or reports a failure and the dock stays up. Recreate the directory: it lists again.

### Grid at scale — `/usr/share/applications`

This is the case deferred here from 5.2.2, which could not be run when it was written because
there was no stack surface to run it in. ~200 entries; the mode exists for this.

- [ ] **It opens without a stall.** **Pass:** the stack appears in well under a second. If a
      listing is still arriving, a loading placeholder is shown rather than an empty grid.
- [ ] **Everything is there.** Compare against `ls /usr/share/applications | wc -l`. **Pass:** the
      counts agree.
- [ ] **Scrolling is smooth.** Flick from top to bottom and back twice. **Pass:** no visible
      stutter, no blank rows that fill in late, no drift in the stack's position.
- [ ] **It is usable, not merely correct.** Pick three applications you know are installed and
      find each by scrolling. **Pass:** each is found in a few seconds, its icon is recognisable
      at `tileSize=64`, and its name is legible — elided at two lines, not one word plus dots.
- [ ] **Clicking one launches it.** **Pass:** the application **starts** and the stack closes. It
      does not open in a text editor — a `.desktop` entry here is an application, not a document.
      This is run 1's finding 2 and the reason `setRunExecutables` is true.
- [ ] **An entry outside the standard directories still asks.** Copy a `.desktop` file into
      `/tmp/frappe-fixture/mixed`, open that stack, and click it. **Pass:** KIO's trust prompt
      appears rather than the entry launching silently. Untrusted is KIO's judgement to make, and
      it must still be making it.
- [ ] **The dock stays responsive with it open.** Hover along the shelf. **Pass:** magnification
      and hover still respond normally; a large stack does not make the dock sluggish.

### Fan view

Right-click the *few* tile → **Fan**.

- [ ] **The menu shows the current mode.** **Pass:** the folder menu lists Grid, Fan and List as
      checkable rows with the current one checked, then the five sort orders in their own group,
      then Show in File Manager and Remove from Dock. No application rows — no Quit, no Pin.
- [ ] **Items are arranged on an arc.** Open *few*. **Pass:** three entries on an arc rising from
      the tile, one cell pitch apart along it, the first straight above the tile. Not a column,
      not a heap.
- [ ] **Entries are named.** **Pass:** each entry's file name is drawn beside its icon, on the
      side the fan sweeps towards, and is legible. A fan of documents is otherwise a row of
      identical page icons. Long names elide in the middle rather than widening the fan.
- [ ] **The names swap sides with the dock.** With `position=Right`: **Pass:** the labels are on
      the left of their icons, so they lean inwards with the arc rather than off the screen.
- [ ] **The arc reaches further, it does not splay wider.** Switch *mixed* to Fan and compare.
      **Pass:** the sweep angle is the same in both; the ten-item fan simply reaches further.
- [ ] **It caps rather than crowds.** With *mixed* in Fan: **Pass:** at most ten entries are
      drawn, evenly spaced — the eleventh is absent rather than squeezed in.
- [ ] **Direction follows the edge.** With `position=Right`, open the fan. **Pass:** it sweeps
      left, into the screen. Nothing leaves the output.
- [ ] **Navigation works here too.** Click `nested` in the fan. **Pass:** the fan redraws with
      `nested`'s contents and the back arrow works.
- [ ] **Every drawn entry is clickable.** Click the first, the middle and the last. **Pass:** all
      three respond — the input region covers the whole arc, including its far end, which is the
      part furthest from the shelf and so the part most likely to fall outside the mask.

### List view

Right-click *mixed* → **List**.

- [ ] **One row per entry, icon then name.** **Pass:** a vertical list, artwork at the leading
      edge, full name beside it. Rows are one cell tall.
- [ ] **The width does not chase the contents.** Note the width, then
      `touch '/tmp/frappe-fixture/mixed/a-very-long-file-name-that-goes-on-and-on.txt'`. **Pass:**
      the list stays the same width and the long name elides.
- [ ] **It scrolls.** **Pass:** about eight rows are visible and the rest scroll; the list does
      not grow to 40 rows.
- [ ] **Navigation.** Enter `nested`, then back. **Pass:** as in Grid — header, breadcrumb and
      back arrow behave identically. The three modes differ only in arrangement.

### Sorting

On *mixed*, whose timestamps were set apart by the fixture. Right-click the tile for each order,
then compare against the equivalent `ls`.

- [ ] **Name.** **Pass:** matches `ls -1 /tmp/frappe-fixture/mixed | sort`, ASCII order, folders
      wherever their names put them.
- [ ] **Date Added.** **Pass:** matches `ls -1c` (ctime — the closest honest answer on Linux; see
      the folder-backend decision record). `file-01.txt` is not first here, because `touch -d`
      moved its mtime, not its ctime.
- [ ] **Date Modified.** **Pass:** matches `ls -1t`, and `file-01.txt` is at the old end.
- [ ] **Date Created.** **Pass:** matches `ls -1 --time=birth -t` where the filesystem records
      birth times; on one that does not, it matches Date Added rather than showing an arbitrary
      order.
- [ ] **Kind.** **Pass:** `nested` is above every file, and the `.png` files are grouped together
      away from the `.txt` files.
- [ ] **Ties break by name.** In Date Modified, the 40 `file-NN.txt` entries share a timestamp to
      the second. **Pass:** they appear in name order within that block, and in the *same* order
      after closing and reopening the stack. A reshuffle between reads is a failure.
- [ ] **The order changes live.** With the stack open, pick a different order from the menu.
      **Pass:** the visible list reorders immediately, without the stack closing or moving.
- [ ] **Sort is per folder.** Set *mixed* to Kind, then open *few*. **Pass:** *few* is still in
      its own order, unaffected.

### Persistence

- [ ] **View mode persists per folder.** Set *mixed* to List and *few* to Fan. Close both. Reopen
      each. **Pass:** *mixed* opens as a list, *few* as a fan.
- [ ] **It survives a restart.** Quit the dock and start it again. **Pass:** both modes are still
      in effect, and `frappe-dockrc` has a `[Stacks]` group with one key per folder.
- [ ] **An untouched folder uses the default.** Open the applications stack, never having chosen
      a mode for it. **Pass:** Grid, and no key for it is written to `[Stacks]` — the file does
      not grow a line for every folder ever opened.
- [ ] **The default sort moves untouched folders only.** Set `[StackDefaults] sort` to Kind
      (Date Added = 1, Kind = 4) while *mixed* has an explicit order of its own. **Pass:**
      folders with no preference switch to Kind; *mixed* keeps its explicit choice.
- [ ] **Removing the tile forgets the folder.** Right-click *few* → Remove from Dock. **Pass:**
      the tile goes, and its key disappears from `[Stacks]` — no stale line for a folder no
      longer on the dock.

### Position under magnification — the §11 regression

Set `magnificationEnabled=true`, `magnificationFactor` at its default. **This is the reason the
anchor is the tile's resting centre**; if any line here fails, the fix has been lost.

- [ ] **No jump on first pointer movement.** Click the *mixed* tile to open the stack. Without
      leaving the tile, move the pointer slowly a few tiles along the dock. **Pass:** the stack
      does **not** shift, at all, at any point — not on the first movement, not as the tile
      shrinks back to rest. This is the defect the reference platform has and this dock must not.
      A displacement of even a few pixels fails.
- [ ] **Sweep the whole dock.** Move from one end of the shelf to the other and back, twice.
      **Pass:** the stack is motionless throughout while the strip magnifies underneath it.
- [ ] **Leaving the dock entirely.** Move the pointer off the shelf so the strip returns to rest.
      **Pass:** still motionless.
- [ ] **Where it sits is defensible.** **Pass:** with magnification on, the stack is centred on
      the tile's *resting* position, so under a swollen tile it may look slightly off-centre.
      That is the accepted trade and is not a failure; a jump is.
- [ ] **Opening while magnified.** With the pointer already on the tile and the tile at full
      magnification, open the stack. **Pass:** it appears in the same place it appears when
      opened from a cold hover — compare the two directly.
- [ ] **Clicks still land while magnified.** With the stack open and the pointer sweeping the
      shelf, click an entry in the stack. **Pass:** it opens. The input region did not move with
      the magnification.
- [ ] **At the extremes of `tileSize`.** Repeat the sweep at `tileSize=24` and at `tileSize=128`.
      **Pass:** no jump at either, and the stack is a uniform scale of itself — same proportions,
      same clearance from the shelf relative to S.

## Results

**Run 1 — 2026-09-03**, nested KWin 1600×900, `tileSize=64`, fixture as above
(`mixed` held 86 entries rather than 40 — `cp` pulled in 45 hicolor PNGs alongside the 40 text
files — and `/usr/share/applications` held 263).

| Section | Result |
|---|---|
| A — opening and closing | Pass, with a gap: see finding 1 |
| B — each dock edge | Pass |
| C — grid view | Pass |
| D — grid at scale | **Fail** at step 29: see finding 2 |
| E — fan view | Pass on arrangement, with a gap: see finding 3 |
| F — list view | Pass |
| G — sorting | Pass |
| H — persistence | Pass |
| I — magnification, the §11 regression | Pass — no jump at any step, all edges |

The §11 regression the checklist exists for **holds**: the stack is motionless under a full
pointer sweep with magnification on, at both extremes of `tileSize`. The anchor decision is
sound and the three findings below are all elsewhere.

### Finding 1 — a stack cannot be dismissed by clicking away from it

Clicking bare desktop leaves the stack open. Two causes, either sufficient on its own:
`StackPopup`'s `closeRequested` is emitted by nothing (`Dock.qml` handles a signal that never
fires), and the input region is `shelfRect | stackRect`, so a click outside both never reaches
the surface to be noticed. Escape does not dismiss either.

Only reachable dismissals are clicking the folder tile again, or activating an entry.

### Finding 2 — `.desktop` entries open as text instead of launching

`LauncherBackend::openUrl()` sets `setRunExecutables(false)`, so `OpenUrlJob` opens a desktop
entry as a document. A stack pointed at `/usr/share/applications` is therefore a Launchpad in
which clicking an application opens a text editor, which is the use case §6 of the design
analysis names as the reason Grid was built first.

### Finding 3 — the fan does not label its entries

Icon-only. `StackFan.qml` declares `required property string name` and never renders it, while
Grid and List both label — a dropped label rather than an omitted one.

### All three fixed, 2026-09-03 — **re-run required**

Fixed under task 5.5.4, each with its regression test written first:

| Finding | Fix | Covered by |
|---|---|---|
| 1 | Dismissal backdrop behind the dock, Escape shortcut, and the input region widened to the output while a stack is open | `tst_stackdismiss.qml`, 8 cases |
| 2 | `setRunExecutables(true)` in `LauncherBackend::openUrl()` | this checklist only — see below |
| 3 | Labels beside the artwork in `StackFan.qml`, elided, side chosen by dock edge | `tst_fanview.qml`, 2 cases |

A fourth defect fell out of writing the tests for finding 1 and is fixed with it: with a
dismissal backdrop in place, a click on the stack's own **padding** fell through it and closed
the stack, so missing an icon by two pixels dismissed the whole thing. `StackPopup` now swallows
clicks over its own rectangle that no entry claimed.

**Finding 2 has no automated test and cannot have a useful one.** Everything on our side of the
call was already correct — the right path reached `openUrl()`, which is all a fake can observe —
and what was wrong was the instruction given to KIO. Asserting it would mean either testing the
fake or launching real applications from the suite. The two new items in section D are its only
coverage, which is what a manual checklist is for.

The three sections that found something — A, D, E — need re-running before Phase 5 closes.

## Regressions this checklist has caught

- **Run 1, finding 2** — `.desktop` entries opened as text rather than launching. Not visible to
  any headless test: `openUrl()` was called correctly and with the right path, and only KIO's
  behaviour on the other side of it was wrong.
- **Run 1, finding 1** — no click-away or Escape dismissal. Structurally invisible to a headless
  test, which has no input region and so cannot notice a click that never arrives.
- **Run 1, finding 3** — fan entries unlabelled. `tst_fanview.qml` asserted arc geometry,
  spacing, capping and lean, and nothing about what a delegate draws.
