# app-identity: host and QNX VM receipt, 2026-11-21

Scope: every closure criterion of the `app-identity` gate, checked item by item against
what ran. **The gate stays `pending`.** Two criteria cannot be met without a device or an
owner decision (listed under "Missing"). Nothing here ran on the Pi 400 or a release board,
and the Pi 400 was not touched.

Source: `main` at `4a4c4818c37c8107f5c92471ed024f61243627cf` for everything labelled host.

| Label         | What it is                                                                                                                                                                                   |
| ------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| host          | macOS 27.0, Homebrew Qt 6.12.0, Node 26.13.0, `QT_QPA_PLATFORM=offscreen`, Apple clang                                                                                                       |
| QNX VM        | `QNX qnxvm 8.0.0 2026/06/05-16:21:14EDT QEMU_virt aarch64le`, QEMU TCG, Screen with the software blitter. Not the Pi 400, not the release board ([vm/describe-app.txt](vm/describe-app.txt)) |
| Pi 400        | nothing new. The historical [tile-layout receipt](../tile-layout-pi400-2026-11-13/README.md) is unchanged and does not cover the icon or the label                                           |
| release board | not run                                                                                                                                                                                      |

## Closure criteria, item by item

State key: **met (host)**, **met (QNX VM)**, **partly met**, **pending (device)**,
**pending (owner decision)**, **missing**.

| #   | Criterion                                                                                                                                               | State                                                                 | Evidence                                                                                                                                                                                                                                                                                                                                                                                                          |
| --- | ------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Read the Expo `icon` field, with the optional `extra.rnqnx.icon` override                                                                               | met (host)                                                            | `tests/js/app-identity.test.mjs` (`iconSetting` precedence, lines 131 to 133). The sample app ([host/positive/app.json](host/positive/app.json)) sets `expo.icon` and prepare-native used it                                                                                                                                                                                                                      |
| 2   | Reject a source that is not a square PNG of at least 1024×1024, in prepare-native and doctor                                                            | met (host)                                                            | prepare-native: 640×640 and 1000×1024 sources and an empty `icon` string exit 1 with the file name and fix ([host/negative/](host/negative/), `exit-codes.txt`). doctor: the JS test covers the same rules (lines 611 to 624); a doctor run on the sample passed `app-icon`                                                                                                                                       |
| 3   | Generate the size set each target needs at build time, list it in the docs, hash it in `native-app.json`                                                | met (host)                                                            | 10 sizes (16 to 512) written for both `marathon` and `qnx`, each with a SHA-256 in `native-app.json` ([host/positive/](host/positive/)). The size table is in `docs/getting-started/app-identity.md`. Both targets get the same set; the docs name which sizes serve which location                                                                                                                               |
| 4a  | Apply to `QGuiApplication::setWindowIcon`                                                                                                               | met (QNX VM)                                                          | `--describe-app` on the VM prints `"windowIconSizes":[16,24,32,48,64,96,128,192,256,512]` ([vm/describe-app.txt](vm/describe-app.txt)). Build: see "QNX VM build" below                                                                                                                                                                                                                                           |
| 4b  | Apply to the title bar                                                                                                                                  | met (QNX VM)                                                          | [vm/chrome-default.png](vm/chrome-default.png), a real render on the VM: the icon sits left of the title `QNX Components`, which is `expo.name`                                                                                                                                                                                                                                                                   |
| 4c  | Apply to the launcher tile                                                                                                                              | pending (device)                                                      | No launcher exists in this repository or the QNX SDP. The Marathon launcher is not on any machine used here. Host mock only (criterion 11)                                                                                                                                                                                                                                                                        |
| 4d  | Apply to the task switcher                                                                                                                              | pending (device)                                                      | No QNX task switcher is known. Marathon's is not measured. Qt's QNX plugin has no `setWindowIcon`, so nothing on screen comes from it (documented, not re-tested here)                                                                                                                                                                                                                                            |
| 4e  | Marathon app manifest                                                                                                                                   | missing                                                               | No `.desktop` or manifest generator exists for consumer apps (`docs/getting-started/app-identity.md`, "Not covered yet"). Not written in this lane: it would be a new component, and its result can only be checked on a Marathon device                                                                                                                                                                          |
| 5   | Ship a default template icon, use it with a one-line notice naming `expo.icon` when none is set                                                         | met (host)                                                            | `template/qnx/assets/icon.png` is a 1024×1024 RGBA PNG. A run with no icon exits 0, prints the notice, and writes 10 icons ([host/negative/no-icon-default.log](host/negative/no-icon-default.log))                                                                                                                                                                                                               |
| 6   | Every prepared app gets an icon set; prepare-native builds the identity tool when missing; fails where it cannot be built; no blank default icon        | met (host), failure branch by test only                               | With no `RNQNX_IDENTITY_TOOL`, prepare-native configured and built `rnqnx_identity_tool` itself and wrote 10 icons ([host/auto-build-identity-tool-prepare-native.log](host/auto-build-identity-tool-prepare-native.log)). `--skip-identity-tool` is rejected (test, lines 503 to 507). The unsupported-host refusal is the CMake guard in `native/app-identity/CMakeLists.txt`; it was not run on a Windows host |
| 7   | Never bake corner masks                                                                                                                                 | met (host)                                                            | All 10 generated icons have alpha 255 at every corner and the source's corner colour ([host/corner-pixels.txt](host/corner-pixels.txt)). [renders/icon-set-contact-sheet.png](renders/icon-set-contact-sheet.png) shows square corners. `IconSetTest.cpp` checks the full-bleed background                                                                                                                        |
| 8a  | Tile-name fit by measured rendered width in the launcher font; label is `shortName` else `expo.name`; `expo.name` stays the title                       | met (host) for Marathon                                               | Real run: `Field Notes Professional` with `shortName: "Notes"` measured 48.2 px in a 90.4 px limit and the title stayed the long name ([host/positive/prep-marathon.log](host/positive/prep-marathon.log))                                                                                                                                                                                                        |
| 8b  | Record the measured label box (width, font, size, line count) for each target launcher                                                                  | partly met                                                            | Marathon: 92.4 px, Source Sans 3 Regular and Light, 20 px, 1 line, derived from upstream source (`native-app.json` `labelFit`). QNX: there is no launcher, so `labelFit.status` is `not-measured`. Neither box was measured on a device                                                                                                                                                                           |
| 9   | Fail with the measured overflow, never truncate; label single-line with elision                                                                         | met (host) for the check; elision in a real launcher pending (device) | `Field Notes Professional` exits 1 with `109.6 px too wide` and a suggestion; `学習ノート` exits 1 with `5.4 px too wide` ([host/negative/](host/negative/)). Elision appears only in the host mock, because this repository owns no launcher QML                                                                                                                                                                 |
| 10  | Tests cover longest fitting name, one glyph over, wide glyphs, CJK, emoji, RTL                                                                          | met (host)                                                            | `LabelFitTest.cpp`: `longestFittingNameAndOneGlyphOver`, `wideGlyphsFitFewerThanNarrowOnes`, `cjkUsesFallbackAndStillMeasures`, `emojiClustersAreOneStep`, `rightToLeftStaysOneLine`. the QTest totals (9, 7 and 16 passed, 0 failed) hold with and without ASan/UBSan; 87 JS checks pass ([host/](host/))                                                                                                        |
| 11  | Host and device receipts show the custom icon in every location, and a tile grid where a maximum-length name and a short name give identical tile sizes | partly met (host mock, QNX VM title bar); device pending              | Host mock: [renders/tile-grid-host-mock.png](renders/tile-grid-host-mock.png) and [.json](renders/tile-grid-host-mock.json). `Notes`, `Calculator`, `WWWWW` (the widest letters that fit) and an overflowing name all get a 98.4 × 116.5 px cell and a 92.4 × 26.5 px label box. VM: title bar only. No device receipt, no launcher render                                                                        |

## What the renders show

Reviewed by looking at the images, not by reading code.

- `renders/icon-set-contact-sheet.png`: the sample icon (an `FN` mark on a teal gradient, full
  bleed) at 16, 24, 32, 48, 64, 96, 128, 192, 256 and 512 px. The mark stays legible at 24 px and
  fuzzy at 16 px. Corners are square at every size.
- `renders/tile-grid-host-mock.png`: four tiles drawn at 3×. The three fitting labels sit inside
  the dotted label box. `Field Notes Professional` elides to `Field No…` and the tile keeps its
  size. The measured width is printed under each tile (48.2, 85.7, 78.6, 200.0 px).
  **This is a mock** of the Marathon label box derived from upstream source, drawn with the repo's
  fonts and `QFontMetricsF::elidedText`. It uses no launcher code. It shows that a box of fixed
  size yields tiles of fixed size, and it cannot show what the launcher does.
- `vm/chrome-default.png`: the showcase app on the QNX VM with the default title bar and macOS
  controls. The icon at the left of the title is readable at the bar's height and the title is the
  full `expo.name`.

## QNX VM build

The VM already held a deployed consumer app, `/data/vm/app/org.rnqnx.showcase`. I did not rebuild
it. It is the PR 149 build (head `240dc0814dd129e806e5228d1b70b9cc27dbd2d6`, cross-built 2026-11-19
in `qnx805-build`, receipt at `/home/qnxbuild/pr149b/RECEIPT.md` in the container), not `main`
`4a4c4818`. The deployed executable has SHA-256
`4f465406f525c86d530146598e38b1e8d17c5149edff71e9fe9349ef810f69cf` (9,016,264 bytes). I did not diff
that source against `main`, so the VM results show that the icon pipeline works on QNX at that
build and are not a statement about `4a4c4818`. The app's icon is `apps/component-gallery/assets/icon.png`
(the React atom), so it is not the sample icon in the host renders.

Commands: `--describe-app` and `--window-size 800x500 --bundle proof-button.hbc --screenshot`, run
with the environment from `/data/vm/capture.sh` (`QT_QPA_PLATFORM=qnx:no-fullscreen`,
`QT_QUICK_BACKEND=software`). The screenshot log ends with
`RNQNX_WINDOW_CHROME platform=qnx controls=macos style=default inset=44 window=1280x800 mode=contained`
([vm/chrome-default.log](vm/chrome-default.log)). The PNG is the 1280×800 screen with an
800×500 window centred on it; the log line reports 1280×800, and I did not trace why it differs
from the requested size.

## Missing, with the owner of each

| Item                                                                    | Needs                                                                                                                                |
| ----------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Launcher tile with the custom icon, and the tile grid with real elision | A device running the Marathon launcher (not the Pi 400 QNX image)                                                                    |
| Task switcher icon                                                      | The same Marathon device; none exists on QNX                                                                                         |
| Marathon app manifest (`.desktop`) written from the icon set            | A generator (new component, owner decision) and a Marathon device                                                                    |
| Measured label box for each target launcher on a device                 | The same device. For QNX, a decision on whether a QNX launcher is in scope; with none, "each target launcher" has no QNX row to fill |
| Host receipt at the candidate commit that includes a rebuilt VM binary  | A `main` cross-build and deploy; not done here                                                                                       |

No gate, budget or acceptance text was loosened.

## Reproduce

```bash
cmake -S native/app-identity -B build/app-identity -G Ninja -DRNQNX_BUILD_APP_IDENTITY_TESTS=ON
cmake --build build/app-identity && ctest --test-dir build/app-identity --output-on-failure
node --test tests/js/app-identity.test.mjs
# render tool (evidence only): render/IdentityRender.cpp, linked with native/app-identity/LabelFit.cpp
```

`SHA256SUMS` lists every file here; check with `shasum -a 256 -c SHA256SUMS`.
