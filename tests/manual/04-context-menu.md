# Manual checklist 04 — context menu and jump lists

`test_contextmenu` proves the rows are assembled correctly and `tst_tilemenu.qml` proves they
render, both against inputs the test chose. What neither can check is whether **real
applications** declare the actions we expect, and whether the rows do what they say against a
live session. Jump lists in particular are only as good as the `Actions=` groups applications
actually ship.

Nested KWin, never the live session:

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

`--no-perm-check` is required or the window list is empty and every window-related row is
missing for the wrong reason. `XDG_CONFIG_HOME` matters more than usual here: **the Open at
Login row writes to `$XDG_CONFIG_HOME/autostart/`**, so without it a test run would add
applications to your real session's startup.

```ini
[General]
tileSize=64
position=Bottom
pinnedEntries=org.kde.konsole,firefox
```

## What to check before starting

Jump list contents come from the desktop entry, so read it first and check the dock against
it rather than against expectation:

```sh
grep -A3 '^\[Desktop Action' /usr/share/applications/firefox.desktop
kreadconfig6 --file /usr/share/applications/firefox.desktop \
             --group 'Desktop Entry' --key Actions
```

An application with no `Actions=` group has no jump list, and a dock showing none for it is
**correct**. That is the distinction this checklist exists to make.

## Checklist

- [ ] **Right-click opens the menu.** Right-click any tile. **Pass:** a menu appears at the
      pointer.
- [ ] **Press and hold opens the same menu.** Hold the left button on the same tile for about a
      second. **Pass:** the identical menu appears, and the application does **not** launch or
      activate when the button is released.
- [ ] **Jump list on a browser.** Right-click Firefox. **Pass:** the menu contains one row per
      `[Desktop Action …]` group in its desktop file, with the same labels and in the same
      order. Choose *New Private Window* (or equivalent). **Pass:** that specific action runs,
      not a plain launch.
- [ ] **Jump list on a media player.** Right-click a media player (Elisa, VLC). **Pass:** its
      declared actions appear. Note in Results **which** player was used and what it declared —
      players vary more than browsers, and some declare nothing.
- [ ] **An application with no actions.** Right-click something with no `Actions=` group.
      **Pass:** no action rows, no empty group, no stray separator at the top of the menu.
- [ ] **Window list appears only with several windows.** One Konsole window: no window rows.
      Open a second: two rows, titled after the windows. **Pass:** both, and choosing a row
      raises *that* window.
- [ ] **Window titles track reality.** Change a window's title (`cd` somewhere in Konsole),
      close the menu, reopen it. **Pass:** the row shows the new title — the menu is rebuilt on
      every open, not cached.
- [ ] **Pin / Unpin.** Right-click an unpinned running application. **Pass:** the row reads
      *Keep in Dock*; choosing it pins the app and the row becomes *Remove from Dock* next time.
      Unpin a pinned running app. **Pass:** the tile stays (it is still running) but is no
      longer pinned, and `frappe-dockrc` reflects both changes.
- [ ] **Unpinning a non-running application removes the tile.** **Pass:** tile gone, config
      updated.
- [ ] **Open at Login.** Check the row. **Pass:** a copy of the desktop file appears in
      `$XDG_CONFIG_HOME/autostart/`, and the row shows as checked next time the menu opens.
      Uncheck it. **Pass:** the file is gone. **Confirm the path is the test one, not
      `~/.config/autostart`.**
- [ ] **Show in File Manager.** **Pass:** a file manager opens with the application's `.desktop`
      file selected — the same behaviour as Meta-click.
- [ ] **Quit.** With two Konsole windows open, choose Quit. **Pass:** both windows close, not
      just one. A pinned tile stays behind with no running indicator; an unpinned one
      disappears.
- [ ] **Quit is absent when nothing is running.** **Pass:** no Quit row on an idle tile.
- [ ] **Force Quit is absent.** **Pass:** no Force Quit row, ever, on any tile. This is
      currently correct: the model supports the row but nothing can report unresponsiveness
      (see the notes on task 2.5.1). If it ever appears, something is setting `isResponsive`
      wrongly.
- [ ] **Menu closes without acting.** Open the menu and press Escape, then click elsewhere.
      **Pass:** it closes and nothing happens — in particular the tile underneath does not
      launch.

## Results

Not yet run. Record which browser and which media player were used, and what each declared.

## Regressions this checklist has caught

(none yet — this checklist has not been executed)
