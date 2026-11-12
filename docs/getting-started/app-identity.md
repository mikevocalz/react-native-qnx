# App icon and tile name

`rnqnx project prepare-native` checks the app icon and the launcher tile label
before it writes `build/<platform>/native-app.json`. `rnqnx project doctor`
runs the same checks without writing anything.

## Config fields

All fields live in static `app.json`; dynamic `app.config.js` is not read.

| Field                        | Required | Meaning                                                                                        |
| ---------------------------- | -------- | ---------------------------------------------------------------------------------------------- |
| `expo.name`                  | yes      | Window title and About name. Also the tile label when `shortName` is absent.                   |
| `expo.icon`                  | yes      | Project-relative path to the icon source.                                                      |
| `expo.extra.rnqnx.icon`      | no       | Overrides `expo.icon` for QNX and Marathon only, so another platform can keep its own icon.    |
| `expo.extra.rnqnx.shortName` | no       | Tile label. Use it when `expo.name` is too wide for the launcher; `expo.name` stays the title. |

```json
{
  "expo": {
    "name": "Field Notes Professional",
    "icon": "./assets/icon.png",
    "extra": {
      "rnqnx": {
        "appId": "org.example.fieldnotes",
        "componentName": "FieldNotes",
        "shortName": "Notes"
      }
    }
  }
}
```

`rnqnx project create` writes `expo.icon` and copies the default icon from
`template/qnx/assets/icon.png`. Replace that file with your own.

## Icon

The source must be a PNG, square, and at least 1024×1024. The check reads the
PNG signature and `IHDR` header, so a renamed JPEG or a corrupt header fails
with the file name and the fix:

```text
expo.icon "./assets/icon.png" is 640x640. Export a square PNG of at least 1024x1024; keep corners square, the platform applies any mask.
```

Keep the corners square and fill the whole square. Neither prepare-native nor
the host applies a mask, and the Marathon launcher at the pinned revision below
(`shell/qml/ui/MAppIcon.qml`) draws the image unmasked. Keep the parts that
identify the app inside the centered circle whose diameter is 66% of the edge,
so the same artwork survives if a launcher later adds a round or rounded-square
mask. The default icon keeps its mark inside that circle.

### Generated sizes

prepare-native scales the source once per build with Qt's smooth
(area-averaging) scaler and writes `build/<platform>/icons/icon-<size>.png`:

| Edge (px)      | Used for                                                     |
| -------------- | ------------------------------------------------------------ |
| 16, 24, 32, 48 | Title bar and task switcher at 1x and 2x                     |
| 64, 96, 128    | Launcher tile at Keel pixel ratio 1 (86 px) and 1.5 (128 px) |
| 192, 256       | Launcher tile at 2x of those sizes                           |
| 512            | Launcher tile at pixel ratio 3 (258 px) and above            |

Each file's SHA-256, and the source's, go into `native-app.json`. The consumer
CMake build refuses an icon whose hash changed, copies the set next to the
executable under `icons/`, and the host calls
`QGuiApplication::setWindowIcon` with all ten sizes. `--describe-app` prints
the loaded sizes as `windowIconSizes`.

## Tile label

The tile label is `expo.extra.rnqnx.shortName` when set, otherwise
`expo.name`. It must fit the target launcher's label box on one line, measured
by rendered width in the launcher font. Character counts are not used: ten
narrow letters can fit where six wide ones do not. A label that does not fit
fails prepare-native with the measured overflow; nothing is shortened for you.

```text
Tile label "Field Notes Pro" (expo.name) is 33.5 px too wide for the Marathon launcher: it measures 125.9 px in Source Sans 3 Regular / Source Sans 3 Light at 20 px, and the label box is 92.4 px wide on one line. Set expo.extra.rnqnx.shortName to a shorter label, for example "Field". expo.name stays the window title.
```

The suggestion is the longest run of whole words that fits. When no whole word
fits, the message asks for a shorter label without proposing one.

### Launcher label boxes

| Target   | Launcher source                                                                   | Font                                                   | Size  | Box width | Lines | Device check                |
| -------- | --------------------------------------------------------------------------------- | ------------------------------------------------------ | ----- | --------- | ----- | --------------------------- |
| Marathon | `patrickjquinn/Marathon-Shell@567f1fa` `shell/qml/components/MarathonAppGrid.qml` | Source Sans 3 Regular and Light; the wider result wins | 20 px | 92.4 px   | 1     | not measured (needs device) |
| QNX      | none known                                                                        | —                                                      | —     | —         | —     | not measured (needs device) |

How the Marathon box is derived, all from upstream source at
`567f1faa8bfdf31611dd401134c293daa160221c` plus Keel (Silica on Qt 6) at
`patrickjquinn/shipwright-open@0df499b`:

- The label is a `Text` with `font.family: Silica.Theme.fontFamily` and
  `font.pixelSize: Silica.Theme.fontSizeTiny`, width
  `iconCell.width - Silica.Theme.paddingSmall`.
- Keel's `fontSizeTiny` base is `round(20 × pixelRatio)` and `paddingSmall` is
  `round(6 × pixelRatio)`. The box uses ratio 1.0, Keel's 540 px phone
  reference, without an ambience font scale.
- The grid spans the screen minus `horizontalPageMargin` (24 px) on each side,
  and `LauncherGrid.js` picks 5 columns at 540 px:
  `(540 − 2 × 24) / 5 − 6 = 92.4` px.
- Silica asks for Sail Sans Pro; Marathon's `marathon-fonts` fontconfig maps it
  to Source Sans 3. Which weight the ambience selects was not confirmed, so the
  check measures Regular and Light and keeps the wider result. Both files are
  copied byte-for-byte from that revision into `native/app-identity/fonts/`.

These numbers come from reading source, not from a screenshot of a running
launcher. On a device with another pixel ratio, ambience font scale or
`appGridColumns` setting the box changes. The device box is
`not measured (needs device)`.

Upstream Marathon wraps a long label to a second line (`maximumLineCount: 2`).
This check is stricter on purpose: one line, so a tile with the longest
allowed name and a tile with a short name stay the same size.

This repository owns no launcher QML, so there is no repo-owned tile that
applies single-line elision. If a launcher is added here, its label must use
`maximumLineCount: 1` and `elide: Text.ElideRight` as a safety net; the
prepare-native check is what keeps elision from ever being needed.

QNX: no launcher is defined in this repository or the QNX SDP, so there is no
label box to measure. prepare-native records
`labelFit.status: "not-measured"` for QNX and does not fail.

### Writing a label that fits

Measured on macOS with Homebrew Qt 6.12.0 against the Marathon box
(92.4 px, 20 px, Source Sans 3):

| Label         | Width (px) | Fits |
| ------------- | ---------- | ---- |
| `Calculator`  | 85.7       | yes  |
| `Messages`    | 79.3       | yes  |
| `Field Notes` | 93.0       | no   |
| `WWWWWW`      | 94.3       | no   |
| `学習ノー`    | 77.3       | yes  |
| `学習ノート`  | 95.8       | no   |
| `Notes 📝`    | 77.2       | yes  |
| `פתקים`       | 53.9       | yes  |

- Prefer one word. Two short words fit only when both are short.
- Capitals and wide letters (W, M) cost more than lowercase.
- Source Sans 3 has no CJK, Hebrew, Arabic or emoji glyphs. Those come from
  the host's fallback fonts, so their measured width depends on the machine
  running prepare-native and may differ on the device. The measurement
  records them in `labelFit.fallbackCodePoints`; leave a few pixels of margin
  for such labels.
- An emoji sequence joined with U+200D counts as one character for fitting,
  but its width still comes from the fallback font.
- Right-to-left labels are measured with Qt's bidi layout, the same engine
  the launcher's `Text` uses.

## The identity tool

prepare-native needs `rnqnx_identity_tool`, a small Qt host program, for the
icon set and the label measurement. Build it once with any Qt 6.11.1 or later:

```bash
cmake -S native/app-identity -B build/identity-tool -DRNQNX_BUILD_IDENTITY_TOOL=ON
cmake --build build/identity-tool
export RNQNX_IDENTITY_TOOL="$PWD/build/identity-tool/rnqnx_identity_tool"
```

Or pass `--identity-tool <path>` to prepare-native and doctor. Without it,
prepare-native stops and prints these commands. doctor checks the icon header
and the label text without the tool, and measures the label only when you pass
`--identity-tool`.

## native-app.json fields

```json
{
  "shortName": "Notes",
  "icon": {
    "source": "/abs/project/assets/icon.png",
    "sha256": "…",
    "sizes": [
      { "size": 16, "path": "/abs/project/build/marathon/icons/icon-16.png", "sha256": "…" }
    ]
  },
  "labelFit": {
    "target": "marathon",
    "status": "measured",
    "launcher": "marathon-shell-567f1fa",
    "fontNames": ["Source Sans 3 Regular", "Source Sans 3 Light"],
    "pixelSize": 20,
    "boxWidthPx": 92.4,
    "lineCount": 1,
    "widthPx": 79.3,
    "fallbackCodePoints": [],
    "deviceMeasured": false
  }
}
```

`build-native` re-checks the icon source hash, every generated icon hash and
the resolved tile label against `app.json`, and asks for prepare-native again
when any of them changed.

## Not covered yet

- No Marathon app-manifest or `.desktop` generator exists for consumer apps,
  so the icon set is not yet written into a launcher manifest. The playground's
  `.desktop` file is hand-written.
- Launcher tile, title bar and task switcher on a device: not run (needs
  device and the public-source Qt build).
- The tile grid with a maximum-length and a short name: not run (needs
  device); this repository has no launcher to render it on the host.
