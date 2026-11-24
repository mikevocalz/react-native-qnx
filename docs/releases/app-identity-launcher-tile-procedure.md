# App identity: launcher tile measurement on the Pi 400

Status: procedure, written 2026-11-24 and not run. No command in this file ran on the Pi 400 when it was written.

Labels: **Pi 400** is `qnxuser@192.168.2.2`, QNX 8.0.0 `RaspberryPi400 aarch64le` ([ADR-0023](../adr/0023-pi400-stands-in-for-device-parity.md)).
**host** is the Mac that runs `ssh`, `scp`, `sips` and the measuring script. **QNX VM** is not used here. Nothing in this procedure
touches a stock Pi 4 QSTI or the Marathon launcher.

## What the gate asks for, and what is missing

The `app-identity` gate in `docs/releases/v1-readiness.json` is `pending`. The parts of its acceptance text that concern the launcher
tile, numbered here so the receipt can cite them:

| #   | Acceptance text (quoted or shortened)                                                                              |
| --- | ------------------------------------------------------------------------------------------------------------------ |
| T1  | "apply it to the launcher tile" (the custom icon, not the platform's blank default)                                |
| T2  | the tile label is `extra.rnqnx.shortName` when set, otherwise `expo.name`                                          |
| T3  | "record the measured label box (width, font, size, line count) for each target launcher"                           |
| T4  | "keep the label single-line with elision"                                                                          |
| T5  | "a tile grid where a maximum-length name and a short name produce identical tile sizes", shown by a device receipt |
| T6  | the icon set on the device is the one hashed in `native-app.json`                                                  |

The 2026-11-24 receipt ([README](../evidence/pi400-v1-gates-2026-11-24/README.md), section App identity) has the title bar, the task
switcher and the dock on the Pi 400. It has no launcher tile: the tile files were installed, `demolauncher` reads tiles only when it
starts, and the restart was refused, so no tile was drawn. T1 to T5 have no Pi 400 evidence. The label box for QNX is
`not-measured` in `docs/getting-started/app-identity.md` ("QNX: none known").

## Rules for this procedure

Part A is observation only. It does not restart `demolauncher`, does not touch `drm-rpi4`, `screen`, `rnqnx-winmgr` or
`/system/etc/startup`, does not reboot, does not kill any process, and does not write under `/system`. Its only writes on the Pi 400
are one screenshot file in `/tmp`, removed at the end. A step that would break one of those rules is in Part B and is marked
**owner-approval-required**. An agent does not run Part B. Mike runs it, or says in the session that an agent may.

If a step fails, stop and record the output. Do not retry with a stronger command.

## Part A: observation only

### A0. Set up the host

```sh
export B=qnxuser@192.168.2.2
export K="-o IdentitiesOnly=yes -i $HOME/qnx-pi400-8.0.5/pr102-ssh/id_ed25519"
export APP=org.rnqnx.playground           # the app whose tile is measured
export PROJ=apps/rn-playground            # its project directory on the host
export R=docs/evidence/app-identity-launcher-tile-$(date +%F)
mkdir -p $R/board $R/frames $R/measure $R/identity
```

### A1. Read the app's identity on the host (host)

The identity the tile should show comes from `build/qnx/native-app.json`, written by `prepare-native`.

```sh
node -e '
const d = JSON.parse(require("fs").readFileSync(process.argv[1], "utf8"));
const i = d.icon.sizes.find((s) => s.size === 128);
console.log(JSON.stringify({ appId: d.appId, displayName: d.displayName, shortName: d.shortName ?? null,
  tileLabel: d.shortName ?? d.displayName, icon128: i, labelFit: d.labelFit ?? null }, null, 2));
' $PROJ/build/qnx/native-app.json | tee $R/identity/expected.json
cp $PROJ/build/qnx/native-app.json $R/identity/native-app.json
```

`expected.json` is what T2 and T6 are checked against: the tile label and the SHA-256 of `icon-128.png`. If `native-app.json` is
missing, run `prepare-native` first (host, no board access).

### A2. Record that the desktop stack is the one that was running (Pi 400, read only)

```sh
ssh $K $B 'uname -a; date; sudo -n pidin ar | egrep "drm-rpi4|screen|winmgr|demolauncher|rnqnx-desktop"' \
  | tee $R/board/processes-before.txt
ssh $K $B 'sudo -n pidin ar | egrep "org\.rnqnx" || echo "no org.rnqnx app process"' | tee $R/board/apps-running.txt
```

This shows which window manager runs the launcher (the 2026-11-23 baseline had `rnqnx-winmgr -s demolauncher,st,gtk` as the parent
of `demolauncher`) and the process IDs. A copy taken at the end (A8) must show the same IDs for `drm-rpi4`, `screen`, the window
manager and `demolauncher`. That equality is the receipt that nothing was restarted.

If no `demolauncher` line appears, stop. The launcher is not running and the capture would show no tiles.

### A3. Read the tile configuration the board holds (Pi 400, read only)

```sh
ssh $K $B 'ls -l /system/etc/desktop_files' | tee $R/board/desktop_files-ls.txt
ssh $K $B 'cat /system/etc/desktop_files/desktop.conf' > $R/board/desktop.conf.txt
ssh $K $B "cat /system/etc/desktop_files/$APP.desktop" > $R/board/$APP.desktop.txt || echo "no .desktop file for $APP" | tee $R/board/no-desktop-file.txt
ssh $K $B 'use demolauncher' > $R/board/demolauncher-usage.txt 2>&1 || true
```

`use` prints a program's usage message and does not run it. `demolauncher-usage.txt` may show options such as a font or a scale. If
it shows nothing useful, keep the file as the record that it was asked.

What these files tell you, and what they do not:

- A `[Launcher]` block with `DesktopFileName=$APP.desktop`, plus the `.desktop` file, shows the board holds a tile definition for the
  app. It does not show a tile is drawn: the launcher reads these files when it starts.
- `Width`, `Height`, `BackgroundWidth` and `BackgroundHeight` in the block are the configured sizes. A tile generated by
  `project desktop-entry` has `Width=120`, `Height=120`, `BackgroundWidth=150`, `BackgroundHeight=140` and `Icon=<app-dir>/icons/icon-128.png`.
  Record the values the board has, and compare them with the pixels in A6. They are configuration, not a measurement.
- If A3 finds no block for the app, the answer to "is the tile drawn" is no, and A4 onwards would only photograph stock tiles.
  Go to Part B or record T1 to T5 as not met.

### A4. Confirm the icon file the tile points at is the built one (Pi 400 read, host hash)

```sh
ICON=$(sed -n 's/^Icon=//p' $R/board/$APP.desktop.txt)
ssh $K $B "cat '$ICON'" > $R/identity/icon-128-on-board.png
shasum -a 256 $R/identity/icon-128-on-board.png | tee $R/identity/icon-128-on-board.sha256
node -p "'expected ' + JSON.parse(require('fs').readFileSync('$R/identity/expected.json', 'utf8')).icon128.sha256"
```

T6 passes for this size when the two hashes are equal. `native-app.json` holds a hash for each of the ten sizes, and the tile uses
only 128, so this step covers the size the tile reads and not the whole set.

### A5. Capture the launcher with the existing tooling (Pi 400 read, host convert)

The launcher is the bottom window of the stack. Any app window that covers it hides the tiles.

First check that no app window is open: `apps-running.txt` from A2 says `no org.rnqnx app process` (the playground, the showcase and
the router example all have ids that start `org.rnqnx`). If one is open, stop and ask Mike. Closing or minimizing someone's running app is a change
to the board, and this part does not make it.

Use the same capture the 2026-11-24 receipt used. `qnxuser` cannot read the framebuffer without `sudo -n`, so
`scripts/qnx-device.sh screenshot` (which runs `screenshot` as `qnxuser`) is not enough on this image (`pi400-desktop-app.md`).

```sh
ssh $K $B 'sudo -n screenshot -file=/tmp/launcher-tile.bmp'
scp -q $K $B:/tmp/launcher-tile.bmp $R/frames/launcher-full.bmp
ssh $K $B 'rm -f /tmp/launcher-tile.bmp'
sips -s format png $R/frames/launcher-full.bmp --out $R/frames/launcher-full.png
shasum -a 256 $R/frames/launcher-full.bmp $R/frames/launcher-full.png | tee $R/frames/launcher-full.sha256
```

`screenshot` reads the display through Screen as a client, the way the 2026-11-23 and 2026-11-24 receipts did. It does not restart
Screen or the driver. The board's HDMI output was black on 2026-11-24, so the frame comes from Screen and not from a display.
The capture size is the display size (`mode=1280x720@60` in `pi400-v1-gates-2026-11-24/board/display-mode.txt`); check the PNG is
1280x720 and record it. A different size means a different mode, and every number below is in the capture's pixels.

Take two captures a few seconds apart and compare their hashes. The wallpaper and the clock text can differ; the tile area must not.
If a tile changes between captures, record that.

### A6. Measure the tile geometry (host)

Open `launcher-full.png` at 100% in an image viewer with a pixel readout. For the app's tile and for every other visible tile, write
down, in capture pixels with `x1` and `y1` exclusive:

- `cell`: the tile's box, background plate included
- `icon`: the icon's box
- `label`: the box around the label text (the tight box around the glyphs, one string per box)
- `label_lines`: how many text lines the label uses

Put them in `$R/measure/boxes.json` and run the script below. It draws the boxes over the capture so a reviewer can check them by
eye, writes a 4x crop of each tile, and states whether all cell sizes are equal. The boxes are hand-read on purpose: the wallpaper is
a photograph, so a background-difference detector would find the wallpaper.

```json
{
  "tiles": [
    {
      "name": "Playground",
      "cell": [0, 0, 1, 1],
      "icon": [0, 0, 1, 1],
      "label": [0, 0, 1, 1],
      "label_lines": 1
    }
  ]
}
```

(The numbers above are placeholders that show the shape.)

```python
#!/usr/bin/env python3
"""Draw hand-measured tile boxes on a launcher capture and report sizes.

usage: measure-tiles.py <capture.png> <boxes.json> <out-dir>

boxes.json: {"tiles": [{"name": "...", "cell": [x0, y0, x1, y1],
  "icon": [x0, y0, x1, y1], "label": [x0, y0, x1, y1], "label_lines": 1}]}
Boxes are pixel coordinates read off the capture at 1:1, x1 and y1 exclusive.
Writes overlay.png, a 4x crop per tile, and tile-geometry.json.
"""
import json, sys
from pathlib import Path
from PIL import Image, ImageDraw

capture, boxes_path, out = sys.argv[1], sys.argv[2], Path(sys.argv[3])
out.mkdir(parents=True, exist_ok=True)
img = Image.open(capture).convert("RGB")
spec = json.load(open(boxes_path))
overlay = img.copy()
draw = ImageDraw.Draw(overlay)
colors = {"cell": (255, 0, 255), "icon": (0, 255, 0), "label": (255, 255, 0)}
rows = []
for t in spec["tiles"]:
    row = {"name": t["name"], "label_lines": t.get("label_lines")}
    for key in ("cell", "icon", "label"):
        x0, y0, x1, y1 = t[key]
        if not (0 <= x0 < x1 <= img.width and 0 <= y0 < y1 <= img.height):
            raise SystemExit(f"{t['name']}.{key} is outside the {img.width}x{img.height} capture")
        draw.rectangle([x0, y0, x1 - 1, y1 - 1], outline=colors[key])
        row[key] = {"box": [x0, y0, x1, y1], "width": x1 - x0, "height": y1 - y0}
    cx0, cy0, cx1, cy1 = t["cell"]
    crop = img.crop((cx0, cy0, cx1, cy1)).resize(((cx1 - cx0) * 4, (cy1 - cy0) * 4), Image.NEAREST)
    crop.save(out / f"tile-{t['name'].replace(' ', '_')}-4x.png")
    rows.append(row)
overlay.save(out / "overlay.png")
cells = {(r["cell"]["width"], r["cell"]["height"]) for r in rows}
labels = {(r["label"]["width"], r["label"]["height"]) for r in rows}
result = {
    "capture": {"file": str(capture), "width": img.width, "height": img.height},
    "tiles": rows,
    "distinctCellSizes": sorted(cells),
    "distinctLabelBoxSizes": sorted(labels),
    "cellSizesIdentical": len(cells) == 1,
}
json.dump(result, open(out / "tile-geometry.json", "w"), indent=2)
print(json.dumps({k: result[k] for k in ("distinctCellSizes", "distinctLabelBoxSizes", "cellSizesIdentical")}))
```

```sh
python3 -I measure-tiles.py $R/frames/launcher-full.png $R/measure/boxes.json $R/measure
```

I ran this script on host against `frames/switcher-with-identity.png` from the 2026-11-24 receipt with one hand-made box, to check
that it runs and writes its three outputs. That image is the task switcher, not the launcher, and the box was an example. The
script has not seen a launcher capture.

What can and cannot be measured from pixels:

- Measured: tile cell, icon and label box sizes in pixels, label line count, whether the label is cut short (an ellipsis or a clipped
  last glyph), and whether the icon is the app's mark.
- Not measured: the label font family and the font size in points or pixels. The capture shows glyphs, not a font name. Record the cap
  height and x-height in pixels from the crop as a size estimate and write `font family: not determined` unless `demolauncher-usage.txt`
  or a read-only look at the board names it. T3 asks for the font and the size, so a receipt without them leaves T3 partly met.

### A7. Compare the tile icon with the built icon (host)

Put the 4x crop beside `icon-128-on-board.png` (also 4x) in one image: `frames/tile-icon-vs-built.png`. If the icon box in the capture is
128x128, also compute the mean absolute difference over the icon box against the PNG composed over its alpha, and write the number in
the README. T1 needs the mark to match by eye. I set no numeric threshold. A numeric rule would be an unmeasured choice, and Mike can
set one when he reads the first receipt.

### A8. Close the receipt (Pi 400 read, host)

```sh
ssh $K $B 'sudo -n pidin ar | egrep "drm-rpi4|screen|winmgr|demolauncher|rnqnx-desktop"' | tee $R/board/processes-after.txt
diff $R/board/processes-before.txt $R/board/processes-after.txt && echo "same processes"
ssh $K $B 'ls /tmp/launcher-tile.bmp 2>&1'          # expect: no such file
(cd $R && find . -type f ! -name SHA256SUMS | sort | xargs shasum -a 256 > SHA256SUMS)
```

Lines of `pidin ar` output that contain the `ssh` and `egrep` processes of the session itself will differ. Compare the process IDs of the
four desktop processes, and say so in the README if the diff shows only session noise.

## Receipt layout

`docs/evidence/app-identity-launcher-tile-<date>/`

| File                                                           | What it proves                                                                                            | Criterion  |
| -------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- | ---------- |
| `README.md`                                                    | Labels, board and build identity, every command's exit status, the pass/fail table below, what is not met | all        |
| `identity/native-app.json`                                     | The identity `prepare-native` produced: appId, names, icon hashes, `labelFit`                             | T2, T6     |
| `identity/expected.json`                                       | The tile label rule applied (`shortName` else `displayName`) and the expected 128 px hash                 | T2, T6     |
| `identity/icon-128-on-board.png`                               | The icon file the tile's `Icon=` line names, as held on the board, with `.sha256` beside it               | T1, T6     |
| `board/desktop.conf.txt`                                       | The `[Launcher]` blocks and sizes the board holds                                                         | T1, T5     |
| `board/<appId>.desktop.txt`                                    | `Name=` (the label the launcher is told to draw) and `Icon=`                                              | T1, T2     |
| `board/desktop_files-ls.txt`                                   | File names and sizes in `/system/etc/desktop_files`                                                       | T1         |
| `board/demolauncher-usage.txt`                                 | What `use demolauncher` says about font, scale or size options, if anything                               | T3         |
| `board/processes-before.txt`, `-after.txt`, `apps-running.txt` | The desktop processes were not restarted during the observation, and no app window covered the launcher   | rules      |
| `frames/launcher-full.png` and `.bmp`                          | The launcher as drawn, as the board's `screenshot` returned it, with hashes                               | T1 to T5   |
| `frames/tile-icon-vs-built.png`                                | The drawn icon beside the built icon                                                                      | T1         |
| `measure/boxes.json`                                           | The hand-read boxes                                                                                       | T3, T5     |
| `measure/overlay.png`, `tile-*-4x.png`, `tile-geometry.json`   | The boxes on the capture, per-tile crops, and the computed sizes                                          | T3, T4, T5 |
| `SHA256SUMS`                                                   | Integrity of every file above                                                                             | all        |

## Pass and fail rules

Each row is evaluated from the receipt files only. The gate does not change on a row passing. It changes only if every criterion
of the whole gate has evidence, and the owner agrees (see "What this does not close").

| #   | Pass when                                                                                                                                                                                      | Fail or not met when                                                                                                                                |
| --- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| T1  | `launcher-full.png` shows a tile for the app, the mark matches `icon-128-on-board.png` by eye in `tile-icon-vs-built.png`, and it is not a generic or blank tile                               | No tile for the app is drawn (record "not drawn", and which of A3's findings explains it), or the tile shows a generic icon                         |
| T2  | The text in the tile equals `tileLabel` in `expected.json`, and `Name=` in the `.desktop` file equals it                                                                                       | They differ. State both strings                                                                                                                     |
| T3  | `label` box width and height and `label_lines` are recorded for the app's tile and for the stock tiles, and the font family and size are either read from the board or marked `not determined` | Not recorded. With the font marked `not determined` the row is **partly met**, and the README says so                                               |
| T4  | A tile whose name is longer than its label box is in the capture, its label is on one line and ends in an ellipsis, and the tile keeps the cell size of the others                             | No overflowing label is on screen (the row is **not tested**, not passed), or the label wraps to a second line or is cut mid-glyph with no ellipsis |
| T5  | At least one tile with a maximum-length name and one with a short name are drawn and `cellSizesIdentical` is `true` in `tile-geometry.json`                                                    | Fewer than two such tiles are on screen (**not tested**), or the cell sizes differ                                                                  |
| T6  | The SHA-256 in A4 equals `icon128.sha256` in `expected.json`                                                                                                                                   | The hashes differ. A different icon is installed than the one built                                                                                 |

"Maximum-length name" has no number for the QNX launcher: no label box is known for it. Part A can say only that some drawn tile
overflows its box or that none does. The box is whatever A6 measures. For T4 and T5, a stock tile with a long name (`Developer Desktop`
is one candidate) can serve as the long name if it elides, and the README must name it as a stock tile and not an app built here.

Even if T1 to T6 all pass, the gate stays `pending` unless the rest of its text has evidence. The text also names the Marathon
launcher ("for each target launcher", "Marathon: app manifest"), and
[the host and VM receipt](../evidence/app-identity-host-vm-2026-11-21/README.md) lists the Marathon device items as pending
on a Marathon device that does not exist here. The Pi 400 receipt covers the QNX row only. If Mike decides the QNX row alone closes
the launcher criteria, that is a change to the gate text and needs its own pull request, as ADR-0023 did.

## Part B: needs a `demolauncher` restart (owner-approval-required)

These steps cannot be done by observation. A tile that is not drawn can only be drawn by restarting `demolauncher`, and a second tile
with a different name can only be added by writing under `/system/etc/desktop_files`. The 2026-11-24 session was refused the
restart. Nothing in Part B has run.

**Do not run Part B without Mike's go-ahead in the session.** The go-ahead has to name the board, say that `demolauncher` may be
restarted and `/system/etc/desktop_files` written, and say whether this covers the revert too.

What the steps are, taken from the existing tile guide ([`pi400-desktop-app.md`](../getting-started/pi400-desktop-app.md), section
Launcher tile):

1. B1 (host). Generate two tiles with `project desktop-entry`, one from an app with a short label and one whose label is longer than
   the QNX label box (the 2026-11-24 `rn-playground` has `shortName` `Playground`; a second prepared app or a second `app.json`
   variant supplies the long name; the generator checks the Marathon box and not a QNX one, because none is known). It writes
   `build/qnx/desktop/` and copies nothing to the board.
2. B2 (Pi 400, writes). Copy the generated files to the board, then as root run `install-tile.sh`. It copies the `.desktop` files and
   appends a `[Launcher]` block to `desktop.conf` after saving `desktop.conf.before-rnqnx-tile` (the 2026-11-24 run used
   `desktop.conf.before-v1g2` in `/data/home/qnxuser/v1g2`).
3. B3 (Pi 400, restarts the launcher). `sudo slay demolauncher; sudo on -d demolauncher -a /system/etc/desktop_files`.
   If the screen stays black, the guide's recovery is to start `fullscreen-winmgr -s demolauncher,st,gtk` and then the launcher. On the
   2026-11-23 baseline the winmgr was `rnqnx-winmgr`, so the recovery command differs and must be read from the A2 output first. HDMI output was
   black on 2026-11-24, so a person at the board cannot confirm the screen either; check by `sudo -n screenshot` over ssh.
4. B4. Run A5 to A8 again on the new state (the processes before and after now differ by design; record both).
5. B5 (Pi 400, writes, restarts the launcher again). Revert: copy `desktop.conf.before-rnqnx-tile` back over `desktop.conf`, delete the
   two `.desktop` files, restart `demolauncher` once more, and capture once more to show the stock tiles are back. The 2026-11-24 run
   did revert the files and did not restart, so the running launcher may still hold the tiles it read at its last start. Say which in
   the README.

What I need from Mike before anyone runs Part B:

- A written go-ahead for B2, B3 and B5 on the board at `192.168.2.2`, with a time when the board can be unattended or attended. A
  restart interrupts the desktop for however long it takes.
- Confirmation that no app is running on it (or permission to close the running one).
- The long name to use for the maximum-length tile, or a decision to measure the QNX box first from a probe tile, because the box
  width is unknown.
- Whether a failed restart may be recovered by an agent (`rnqnx-winmgr` restart, which touches the process that runs the desktop)
  or only by Mike. Until he says so, a failed restart ends the run and the board is left as it is.
- A decision on whether the launcher criteria close on the QNX row alone (see above).

## What this does not close

- Part A cannot show a tile the launcher has not read. If the board's launcher started before the app's `[Launcher]` block was
  written, A5 shows no tile and T1 to T5 stay open until Part B runs.
- Nothing here measures the Marathon launcher, the task switcher or `QGuiApplication::setWindowIcon`. The 2026-11-24 receipt covers the
  Pi 400's switcher and title bar; no receipt reads the window icon back.
- The numbers are capture pixels at `1280x720@60`. A different mode or pixel ratio changes them.
