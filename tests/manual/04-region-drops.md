# Manual checklist 04 — region drops onto the shelf

`test_droprules` proves the decision: what each payload is, which region it belongs in, and
that every refusal names a reason and a place. `tst_droprules.qml` proves the states appear —
it posts synthesised drag events, so it can check that the strip marks itself accepted or
rejected and that the sentence shows and clears with the drag.

Neither can answer the criterion this checklist exists for: **is the feedback actually
comprehensible?** A message can be present, correct, and still read as a complaint. Judge the
sentences here as a user meeting them for the first time, not as their author.

Everything runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

Open Dolphin in the nested session, at `/usr/share/applications` for the entries and at your
home directory for the files.

## Accepting

- [ Y ] Drag a `.desktop` from `/usr/share/applications` over the shelf, **beside** the tiles.
      The shelf's rim brightens before the release, and no message appears — an accepted drop
      explains nothing, because there is nothing to explain.
- [ Y ] Release. The application is pinned, and it lands where the pointer was rather than at a
      fixed end.
- [ Y ] Drag the same entry over the middle of an **existing tile**. It is still an application
      being added: the strip answers, the tile underneath does not light up as if it were being
      asked to open a document.
- [ Y ] Drop an entry that is **already pinned**. Nothing is duplicated.
- [ N ] Quit and relaunch. The pinned application is still there.

## Refusing, in words

For each, read the sentence aloud before ticking. It should say what would have to change,
not merely that something is wrong.

- [ Y (the reason is being cut off by the margin of the dock, meaning it does not render outside of the dock area) ] Drag a **file** over the shelf. Refused, with "Files and folders don't go in the
      applications area".
- [ Y (same mention as above) ] Drag a **folder**. Same refusal — a folder is a file as far as the region is concerned,
      and the wording already says so.
- [ Y ] Drag a **mixed selection** — one `.desktop` and one file — over the shelf. "Drop
      applications and files separately".
- [ ] Drag a `.desktop` that is **not an installed application** (copy one to `/tmp` and edit
      its `Exec` to something absent). The message names *that* entry, not a generic failure.
- [ Y ] In each case, release anyway. Nothing is pinned, nothing crashes, and the message goes
      away with the drag.
- [ Y ] Drag over the shelf and then off it without releasing. The message leaves with the
      pointer; it never outlives the drag it is about.
- [ N (the messeage does not display outside of the dock area and it is being cut off)] The messages are legible against the shelf — over the glass, over a bright wallpaper, and
      over a dark one.

## Under the rest of the dock

- [ Y ] Repeat one accept and one refusal with **magnification enabled**. The message stays put
      while the strip moves under it.
- [ ] Repeat with the dock on the **left** edge and on the **right**. The message appears beside
      the shelf, on the screen, never clipped by the edge.
- [ ] Repeat with a **very small** tile size and a **very large** one. The message is the same
      size relative to the shelf in both, and does not overlap the tiles it is about.
- [ Y ] After a refused drop, ordinary clicking, dragging and reordering still work.
