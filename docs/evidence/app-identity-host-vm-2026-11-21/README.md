# app-identity: host and QNX VM receipt, 2026-11-21

Scope: every closure criterion of the `app-identity` gate, checked item by item against
what ran. **The gate stays `pending`.** Several criteria cannot be met without a device
(listed under "Missing"). Nothing here ran on the Pi 400 or a release board, and the Pi 400
was not touched.

Update 2026-11-21, after owner decision ([ADR-0022](../../adr/0022-marathon-launcher-entry-generator.md)):
item 4e was `missing` and is now generated and host-checked; see "Addendum: 4e" below. The
rest of this receipt is unchanged and still describes `main` at `4a4c4818`.

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

| #   | Criterion                                                                                                                                               | State                                                                 | Evidence                                                                                                                                                                                                                                                                                                                                                                                                                        |
| --- | ------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1   | Read the Expo `icon` field, with the optional `extra.rnqnx.icon` override                                                                               | met (host)                                                            | `tests/js/app-identity.test.mjs` (`iconSetting` precedence, lines 131 to 133). The sample app ([host/positive/app.json](host/positive/app.json)) sets `expo.icon` and prepare-native used it                                                                                                                                                                                                                                    |
| 2   | Reject a source that is not a square PNG of at least 1024×1024, in prepare-native and doctor                                                            | met (host)                                                            | prepare-native: 640×640 and 1000×1024 sources and an empty `icon` string exit 1 with the file name and fix ([host/negative/](host/negative/), `exit-codes.txt`). doctor: the JS test covers the same rules (lines 611 to 624); a doctor run on the sample passed `app-icon`                                                                                                                                                     |
| 3   | Generate the size set each target needs at build time, list it in the docs, hash it in `native-app.json`                                                | met (host)                                                            | 10 sizes (16 to 512) written for both `marathon` and `qnx`, each with a SHA-256 in `native-app.json` ([host/positive/](host/positive/)). The size table is in `docs/getting-started/app-identity.md`. Both targets get the same set; the docs name which sizes serve which location                                                                                                                                             |
| 4a  | Apply to `QGuiApplication::setWindowIcon`                                                                                                               | met (QNX VM)                                                          | `--describe-app` on the VM prints `"windowIconSizes":[16,24,32,48,64,96,128,192,256,512]` ([vm/describe-app.txt](vm/describe-app.txt)). Build: see "QNX VM build" below                                                                                                                                                                                                                                                         |
| 4b  | Apply to the title bar                                                                                                                                  | met (QNX VM)                                                          | [vm/chrome-default.png](vm/chrome-default.png), a real render on the VM: the icon sits left of the title `QNX Components`, which is `expo.name`                                                                                                                                                                                                                                                                                 |
| 4c  | Apply to the launcher tile                                                                                                                              | pending (device)                                                      | No launcher exists in this repository or the QNX SDP. The Marathon launcher is not on any machine used here. Host mock only (criterion 11)                                                                                                                                                                                                                                                                                      |
| 4d  | Apply to the task switcher                                                                                                                              | pending (device)                                                      | No QNX task switcher is known. Marathon's is not measured. Qt's QNX plugin has no `setWindowIcon`, so nothing on screen comes from it (documented, not re-tested here)                                                                                                                                                                                                                                                          |
| 4e  | Marathon app manifest                                                                                                                                   | partly met: generator met (host); device pending                      | `project marathon-entry` writes the `.desktop` entry Marathon-Shell reads for native apps. Upstream's own `read_app` and `fits_a_phone`, built unchanged, read the generated file as a shown, phone-ready app named by the tile label ([marathon-entry/](marathon-entry/), [ADR-0022](../../adr/0022-marathon-launcher-entry-generator.md)). Not installed on a Marathon device; the tile, its icon and the launch are not seen |
| 5   | Ship a default template icon, use it with a one-line notice naming `expo.icon` when none is set                                                         | met (host)                                                            | `template/qnx/assets/icon.png` is a 1024×1024 RGBA PNG. A run with no icon exits 0, prints the notice, and writes 10 icons ([host/negative/no-icon-default.log](host/negative/no-icon-default.log))                                                                                                                                                                                                                             |
| 6   | Every prepared app gets an icon set; prepare-native builds the identity tool when missing; fails where it cannot be built; no blank default icon        | met (host), failure branch by test only                               | With no `RNQNX_IDENTITY_TOOL`, prepare-native configured and built `rnqnx_identity_tool` itself and wrote 10 icons ([host/auto-build-identity-tool-prepare-native.log](host/auto-build-identity-tool-prepare-native.log)). `--skip-identity-tool` is rejected (test, lines 503 to 507). The unsupported-host refusal is the CMake guard in `native/app-identity/CMakeLists.txt`; it was not run on a Windows host               |
| 7   | Never bake corner masks                                                                                                                                 | met (host)                                                            | All 10 generated icons have alpha 255 at every corner and the source's corner colour ([host/corner-pixels.txt](host/corner-pixels.txt)). [renders/icon-set-contact-sheet.png](renders/icon-set-contact-sheet.png) shows square corners. `IconSetTest.cpp` checks the full-bleed background                                                                                                                                      |
| 8a  | Tile-name fit by measured rendered width in the launcher font; label is `shortName` else `expo.name`; `expo.name` stays the title                       | met (host) for Marathon                                               | Real run: `Field Notes Professional` with `shortName: "Notes"` measured 48.2 px in a 90.4 px limit and the title stayed the long name ([host/positive/prep-marathon.log](host/positive/prep-marathon.log))                                                                                                                                                                                                                      |
| 8b  | Record the measured label box (width, font, size, line count) for each target launcher                                                                  | partly met                                                            | Marathon: 92.4 px, Source Sans 3 Regular and Light, 20 px, 1 line, derived from upstream source (`native-app.json` `labelFit`). QNX: there is no launcher, so `labelFit.status` is `not-measured`. Neither box was measured on a device                                                                                                                                                                                         |
| 9   | Fail with the measured overflow, never truncate; label single-line with elision                                                                         | met (host) for the check; elision in a real launcher pending (device) | `Field Notes Professional` exits 1 with `109.6 px too wide` and a suggestion; `学習ノート` exits 1 with `5.4 px too wide` ([host/negative/](host/negative/)). Elision appears only in the host mock, because this repository owns no launcher QML                                                                                                                                                                               |
| 10  | Tests cover longest fitting name, one glyph over, wide glyphs, CJK, emoji, RTL                                                                          | met (host)                                                            | `LabelFitTest.cpp`: `longestFittingNameAndOneGlyphOver`, `wideGlyphsFitFewerThanNarrowOnes`, `cjkUsesFallbackAndStillMeasures`, `emojiClustersAreOneStep`, `rightToLeftStaysOneLine`. the QTest totals (9, 7 and 16 passed, 0 failed) hold with and without ASan/UBSan; 87 JS checks pass ([host/](host/))                                                                                                                      |
| 11  | Host and device receipts show the custom icon in every location, and a tile grid where a maximum-length name and a short name give identical tile sizes | partly met (host mock, QNX VM title bar); device pending              | Host mock: [renders/tile-grid-host-mock.png](renders/tile-grid-host-mock.png) and [.json](renders/tile-grid-host-mock.json). `Notes`, `Calculator`, `WWWWW` (the widest letters that fit) and an overflowing name all get a 98.4 × 116.5 px cell and a 92.4 × 26.5 px label box. VM: title bar only. No device receipt, no launcher render                                                                                      |

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
| Marathon launcher entry (`.desktop`) installed and seen on a device     | A Marathon device. The generator exists (addendum below); nothing has loaded its output there                                        |
| Measured label box for each target launcher on a device                 | The same device. For QNX, a decision on whether a QNX launcher is in scope; with none, "each target launcher" has no QNX row to fill |
| Host receipt at the candidate commit that includes a rebuilt VM binary  | A `main` cross-build and deploy; not done here                                                                                       |

No gate, budget or acceptance text was loosened.

## Addendum: 4e, Marathon app manifest (2026-11-21)

Owner decision, 2026-11-21: "nothing missing, so yes to marathon". It lifts the Marathon
freeze for this one item and no other ([ADR-0022](../../adr/0022-marathon-launcher-entry-generator.md)
has the quote, the scope lifted and what stays frozen).

Source: `main` at `bca5a18b` plus the branch for this change. Labels: **host** is macOS 27.0,
Node 26.13.0, Homebrew Qt 6.12.0 (identity tool), Rust (cargo) for the upstream check. There is
no QNX VM, Pi 400 or release board run in this addendum, and **no Marathon device**.

What exists: `pnpm qnx project marathon-entry`
(`packages/react-native-qnx-cli/src/marathon-entry.mjs`), documented in
`docs/getting-started/app-identity.md`. It writes `<appId>.desktop`, the format
Marathon-Shell's `read_app` reads (`crates/marathon-shell-native/src/desktop.rs`,
patrickjquinn/Marathon-Shell `567f1faa`). The signed `.marathon` manifest cannot start a
native executable (its schema requires a `.qml` entry point), so the entry is a `.desktop`
file.

| Check (all host)                                                                                                                                                                            | Result                                                                                                                                                                                                                           | Evidence                                                                                                            |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| JS tests, red first                                                                                                                                                                         | Module absent: `ERR_MODULE_NOT_FOUND`, exit 1. Then green, exit 0. Three mutations of the finished module each fail the test (exit 1): drop the form-factor hint, allow a leading `/` in `Exec`, take the label from `expo.name` | [logs/](marathon-entry/logs/)                                                                                       |
| prepare-native on the sample app, `--platform marathon`, then `marathon-entry`                                                                                                              | Exit 0. `Name=Notes` (`shortName`), the long `expo.name` is not used; `Icon` ends `icons/icon-128.png`; hash `85e7a5bf95b1637a41826368ad100f2d1787386849dd8bb20a28a76e1d957504`                                                  | [generated/](marathon-entry/generated/)                                                                             |
| Determinism                                                                                                                                                                                 | Two runs on the same descriptor: identical file hash and identical output JSON                                                                                                                                                   | `generated/marathon-entry-output.json` and the JS test                                                              |
| Upstream reader, run on the generated file. Upstream `read_app`, `clean_exec`, `fits_a_phone` copied unchanged from `567f1faa`, built against `freedesktop_entry_parser` 2.0.1 (Cargo.lock) | `id=org.example.fieldnotes`, `name=Notes`, `exec` unchanged, `shown=true`, `keel_hosted=false`, `fits_a_phone=true`, `missing_icon=None` (icon dir present on the host), exit 0                                                  | [rust-upstream-check/upstream-read-app-output.txt](marathon-entry/rust-upstream-check/upstream-read-app-output.txt) |
| Same checker, controls                                                                                                                                                                      | Without `X-Purism-FormFactor`: `fits_a_phone=false`, exit 1. With `Exec=/abs/run.sh`: Marathon runs `run.sh`. With the icon file absent: `icon=layout-grid`, exit 1. So the checker fails when the entry is wrong                | same file                                                                                                           |
| Refusal                                                                                                                                                                                     | `--exec /usr/bin/fieldnotes` exits 1 and says Marathon drops the directory                                                                                                                                                       | `generated/reject-leading-slash-exec.txt`                                                                           |

What the upstream check does not cover: the checker copies the reader, not the launcher. It
says nothing about how `MarathonAppGrid.qml` draws the tile, whether a 128 px icon looks right at
the device's pixel ratio, whether the entry's directory is on the scan path of a given image,
or whether tapping the tile starts the host. The `Exec` rules and the allowed characters are a
port of `clean_exec` plus a precaution; nothing in the source says what runs the command.
`StartupWMClass` is not read by `desktop.rs`; that the host reports this id is not verified.

**Device half of 4e: pending (Marathon device).** Needed: install the entry and the icon
directory, confirm the tile shows the custom icon and the `Name` label, tap it and confirm the
host starts. The task switcher (4d), launcher tile (4c), the measured label box (8b) and real
elision (9) stay pending on that same device. The gate stays `pending`.

## Reproduce

```bash
cmake -S native/app-identity -B build/app-identity -G Ninja -DRNQNX_BUILD_APP_IDENTITY_TESTS=ON
cmake --build build/app-identity && ctest --test-dir build/app-identity --output-on-failure
node --test tests/js/app-identity.test.mjs
# render tool (evidence only): render/IdentityRender.cpp, linked with native/app-identity/LabelFit.cpp
# 4e, Marathon launcher entry
node tests/js/marathon-entry.test.mjs
pnpm qnx project prepare-native --project <app> --platform marathon
pnpm qnx project marathon-entry --project <app> --app-dir <device dir> --exec "sh <device dir>/run.sh"
# upstream check: marathon-entry/rust-upstream-check (cargo build; run the binary on a .desktop file)
```

`SHA256SUMS` lists every file here; check with `shasum -a 256 -c SHA256SUMS`.
