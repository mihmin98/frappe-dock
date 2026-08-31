# Manual checklist 04 — file drops onto tiles

`test_filedrop` proves the decision and the call it builds: which MIME types an entry accepts,
that every file must be supported, and that `openWith` is called once with all of them. What it
cannot do is drag a real file from a real file manager across a compositor — the drag protocol,
the URL list that arrives, and whether the application actually opens what it was given are all
outside the process.

`tst_droprules.qml` covers the on-screen states with synthesised drag events, including the
tile's accept highlight and refusal tint. What it cannot synthesise is the rest: a real drag
from a real file manager, and whether the application opens what it was handed.

Everything runs in **nested KWin**, never against the live session.

```sh
cmake --build build -j$(nproc)
XDG_CONFIG_HOME=/tmp/frappe-run/config \
  ./tools/run-nested.sh --no-perm-check ./build/bin/frappe-dock
```

Pin a PDF reader (Okular), an image editor (Gwenview or GIMP) and a terminal emulator. Open
Dolphin in the nested session for the drags.

## Accepting

- [ ] Drag a **PDF over the reader's tile**. The tile lifts — a light rounded highlight behind
      the artwork — before the release.
- [ ] Release. The reader opens that document.
- [ Y ] Drag an **image over the editor's tile** and release. The editor opens it.
- [ Y ] Select **several images** and drop them together. They open in **one** invocation — one
      window with several documents, not one window per file.
- [ Y ] Drop a `.sh` on a text editor that declares `text/plain`. Accepted: a shell script is
      plain text.
- [ Y ] Drag over a tile and then away without releasing. The highlight goes with the pointer.

## Refusing, visibly

- [ Y ] Drag a **.deb (or anything the app does not declare) over a text editor**. The tile tints
      red and a message appears reading "*Editor* can't open *Debian package*".
- [ Y ] The message names the application and the file's type in words — not a MIME string, not a
      generic "invalid".
- [ N (the app still opens as if clicked without a file) ] Release anyway. Nothing opens, nothing crashes, and the message goes away.
- [ Y ] Drag a mixed selection — one PDF and one image — over the reader. Refused as a whole,
      naming the image's type. **Nothing is opened**: a partial open is what this rule exists to
      prevent.
- [ Y ] Drop a file on the **terminal emulator**, which declares no MIME types. Refused with the
      same explanation rather than silently ignored.
- [ ] Drop a file on a **separator** and on a **minimized-window tile**. Neither accepts; neither
      misbehaves.

## Under the rest of the dock

- [ Y ] Repeat one accept and one refusal with **magnification enabled**. The highlight follows
      the tile it is about, and the message stays over that tile as the strip magnifies.
- [ ] Repeat with the dock on the **left** edge: the message appears beside the shelf, on the
      screen, not off the edge.
- [ Y ] After a drop, ordinary clicking and dragging still work — the drop did not leave a grab
      behind.
