# App icon and tile name

`rnqnx project prepare-native` checks the app icon and the launcher tile label
before it writes `build/<platform>/native-app.json`. `rnqnx project doctor`
runs the same checks without writing anything.

## Config fields

prepare-native resolves the config through `@expo/config`, the same way
`expo start` and `expo prebuild` do: `app.json`, `app.config.js` or
`app.config.ts`, with config plugins applied. Options on the
`@rn-qnx/expo-qnx-config-plugins` entry in `plugins` win over values already in
`extra.rnqnx`, so `icon` and `shortName` can come from either place.

```json
{
  "expo": {
    "name": "Field Notes Professional",
    "plugins": [
      [
        "@rn-qnx/expo-qnx-config-plugins",
        {
          "appId": "org.example.fieldnotes",
          "componentName": "FieldNotes",
          "icon": "./assets/icon.png",
          "shortName": "Notes"
        }
      ]
    ]
  }
}
```

| Field                        | Required | Meaning                                                                                        |
| ---------------------------- | -------- | ---------------------------------------------------------------------------------------------- |
| `expo.name`                  | yes      | Window title and About name. Also the tile label when `shortName` is absent.                   |
| `expo.icon`                  | no       | Project-relative path to the icon source. Unset: the default icon (see below).                 |
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

Every prepared app gets a generated icon set. When neither `expo.icon` nor
`expo.extra.rnqnx.icon` is set, prepare-native uses
`template/qnx/assets/icon.png` from the checkout, the way Expo falls back to
its default icon, and prints one line:

```text
rnqnx: notice: no app icon is set, so the default icon template/qnx/assets/icon.png is used. Set expo.icon to a square PNG of at least 1024x1024 to use your own.
```

A field that is set is checked strictly and never replaced by the default: an
empty string, a non-string, a missing file or a PNG that fails the checks
below stops prepare-native.

## Icon

The source must be a PNG, square, at least 1024×1024 and at most 8192×8192. The
check reads the
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
the loaded sizes as `windowIconSizes`. prepare-native writes each icon under a
temporary name and renames it into place, and refuses a destination that is a
symlink.

If the staged set is missing or damaged at launch, the app prints
`RNQNX_APP_ICON_WARNING` and keeps running with the platform's default icon.

Where the window icon shows up depends on the platform plugin:

- Marathon (Qt Wayland): Qt passes the window icon to the compositor; whether
  the Marathon shell shows it in the title bar or task switcher is not measured
  (needs device).
- QNX: Qt's QNX platform plugin (`QQnxWindow`, Qt 6.12.0) has no
  `setWindowIcon`. On QNX the icon reaches `QGuiApplication::windowIcon()` and
  the `--describe-app` output, and nothing on screen.

## Tile label

The tile label is `expo.extra.rnqnx.shortName` when set, otherwise
`expo.name`. It must fit the target launcher's label box on one line, measured
by rendered width in the launcher font. Character counts are not used: ten
narrow letters can fit where six wide ones do not. A label that does not fit
fails prepare-native with the measured overflow; nothing is shortened for you.

```text
Tile label "Field Notes Pro" (expo.name) is 35.5 px too wide for the Marathon launcher: it measures 125.9 px in Source Sans 3 Regular / Source Sans 3 Light at 20 px, and the label box is 92.4 px wide on one line with 2 px kept free. Set expo.extra.rnqnx.shortName to a shorter label, for example "Field". expo.name stays the window title.
```

The check keeps 2 px of the box free, so a label fits when it measures at most
90.4 px. The margin absorbs small differences between this host measurement
and the device's font rendering. Text is laid out unhinted
(`QFont::PreferNoHinting`) with design metrics, as Qt Quick's `Text` does by
default. For the golden labels in `LabelFitTest.cpp`, macOS (Qt 6.12.0) and
Linux (Qt 6.11.1) give the same widths to 0.01 px; a few words with kerning
pairs, such as `Calculator`, differ by up to 1 px between those hosts. Each
face is selected by its file: the weight comes from the font's OS/2 table, and
the measurement stops if Qt resolves a face to a different file. The suggestion is the longest run of whole words that fits
and contains no `<` or `&`. When none fits, the message asks for a shorter label
without proposing one.

Rules for the label text, checked without the tool:

- 1-128 characters on one line, no surrounding whitespace, no control
  characters.
- No invisible format characters (Unicode category Cf), such as U+200B zero
  width space, U+202E right-to-left override, U+00AD soft hyphen or tag
  characters. The one exception is U+200D zero width joiner between two emoji,
  as in 👩‍💻.
- At least one visible character: a label made only of spaces or combining
  marks is refused.

With the tool, a label that Qt would treat as rich text (`Qt::mightBeRichText`,
for example `<b>Notes</b>`) is refused. Marathon shows labels in a QML `Text`
with the default automatic format, which would render the tags as formatting,
so the plain-text width measured here would not be what appears. A plain `&` is
measured as a literal character.

### Launcher label boxes

| Target   | Launcher source                                                                   | Font                                                   | Size  | Box width                               | Lines | Device check                |
| -------- | --------------------------------------------------------------------------------- | ------------------------------------------------------ | ----- | --------------------------------------- | ----- | --------------------------- |
| Marathon | `patrickjquinn/Marathon-Shell@567f1fa` `shell/qml/components/MarathonAppGrid.qml` | Source Sans 3 Regular and Light; the wider result wins | 20 px | 92.4 px (90.4 px after the 2 px margin) | 1     | not measured (needs device) |
| QNX      | none known                                                                        | —                                                      | —     | —                                       | —     | not measured (needs device) |

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
This check is stricter on purpose, as a product decision: one line, so a tile
with the longest allowed name and a tile with a short name stay the same size.

This repository owns no launcher QML, so there is no repo-owned tile that
applies single-line elision. If a launcher is added here, its label must use
`maximumLineCount: 1` and `elide: Text.ElideRight` as a safety net; the
prepare-native check is what keeps elision from ever being needed.

QNX: no launcher is defined in this repository or the QNX SDP, so there is no
label box to measure. prepare-native records
`labelFit.status: "not-measured"` for QNX and does not fail.

### Writing a label that fits

Measured on macOS with Homebrew Qt 6.12.0 against the Marathon box
(92.4 px less the 2 px margin, 20 px, Source Sans 3). The Latin rows are also
golden values in `LabelFitTest.cpp`:

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

prepare-native uses `rnqnx_identity_tool`, a small Qt host program, for the
icon set and the label measurement. It looks for the tool in this order:

1. `--identity-tool <path>`
2. `RNQNX_IDENTITY_TOOL`
3. `build/identity-tool/rnqnx_identity_tool` in the source checkout (moved with
   `RNQNX_IDENTITY_TOOL_BUILD_DIR`)

When none exists, prepare-native builds the third one itself with
`cmake -S native/app-identity -B build/identity-tool -DRNQNX_BUILD_IDENTITY_TOOL=ON`
and `cmake --build`, then reuses it on later runs. CMake finds Qt the same way
the native build does, so set `CMAKE_PREFIX_PATH` when Qt is not in a default
location. If the build fails, prepare-native prints the last CMake lines and
what it needs, and writes no `native-app.json`.

| Host                         | Support                                                                                          |
| ---------------------------- | ------------------------------------------------------------------------------------------------ |
| Linux, macOS                 | Built on first use with CMake 3.31.9 or later and Qt 6.11.1 or later (Core, Gui).                |
| Windows and any other host   | The tool's CMake refuses to configure, so prepare-native fails. Run it on a Linux or macOS host. |
| Cross-compiling environments | The tool is a host program; build it with a native host compiler, never the target one.          |

To build it ahead of time:

```bash
cmake -S native/app-identity -B build/identity-tool -DRNQNX_BUILD_IDENTITY_TOOL=ON
cmake --build build/identity-tool
```

There is no way to prepare an app without an icon set. The
`--skip-identity-tool` option, which used to write `icon.sizes: []` and let the
app run with the platform's blank default icon, was removed; passing it now
fails with a message saying so. The consumer CMake build also refuses a
`native-app.json` whose icon set is empty, and `build-native` treats one as
stale. `rnqnx project run` lets prepare-native find or build the tool, and
passes `RNQNX_IDENTITY_TOOL` on when it is set.

The label fonts are read from `native/app-identity/fonts/` in the source
checkout. The CLI is a source-checkout tool today; if it runs somewhere those
files are absent, the label is recorded as not measured instead of failing.

doctor never runs app config code. It reads static `app.json` and applies the
`icon` and `shortName` options of the `@rn-qnx/expo-qnx-config-plugins` entry
as JSON, following the plugin's rule. When the project has `app.config.js` or
`app.config.ts`, doctor reports the icon and label as not checked and leaves
them to prepare-native, which resolves that code. The one binary doctor can run
is the one passed with `--identity-tool`, to measure the label.

doctor applies the same icon and tool rules. A missing icon is reported as
`passed` with `observed: "default icon, 1024x1024 PNG"` and a message naming
`expo.icon`. The `identity-tool` check is `passed` when prepare-native would
find a tool, `not-checked` when prepare-native would build one first (doctor
never runs a compiler), and `failed` on a host where it cannot be built.

### Reruns on another host

Icon pixels and measured widths can differ slightly between Qt versions and
font engines. When `native-app.json` already exists, prepare-native compares
only the host-independent fields: identity, bundle hash, short name, icon
source and its hash, icon sizes and paths, and the label box. The icon file
hashes, `labelFit.widthPx` and `labelFit.fallbackCodePoints` are left out of
that comparison. The existing icon files are then checked against the hashes
the existing file records. If everything matches, the existing descriptor and
icons are kept and the rerun reports `unchanged`.

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
    "marginPx": 2,
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
