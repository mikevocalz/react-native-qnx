# Pi 400 gate attempt, 2026-11-23

Target: **Pi 400** (`qnxuser@192.168.2.2`, QNX 8.0.0 `RaspberryPi400 aarch64le`, stock `drm-rpi4` and
`screen` left running). This is not a stock Pi 4 QSTI and not the release board. Nothing here
counts for `cold-boot`.

**Result: no gate changed status.** `window-chrome`, `app-identity`, `performance`, `expo` and
`device-parity` stay `pending`. `docs/releases/v1-readiness.json` is untouched.

## Why nothing could be closed today

Every one of the five gates needs a QNX binary built from current `main` (`d85b13dd`) and run on
the board. No such binary can be produced from this host without breaking a rule of this run:

| Check                                                          | Finding                                                                                                                                                                                                                 |
| -------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| QNX SDP on the host                                            | `~/qnx-pi400-8.0.5/qnx800/host` holds `linux`, `win64` and `common` only, so there is no macOS cross compiler                                                                                                           |
| Qt for QNX (headers, CMake config) outside the build container | none found under `~/qnx-pi400-8.0.5`; only the runtime tarballs (`qt612-runtime`, 121 MB) and the Qt source (305 MB). The Qt for QNX build tree used by every earlier receipt lives inside the container `qnx805-build` |
| Container `qnx805-build`                                       | off limits for this run (another lane owns it)                                                                                                                                                                          |
| Rebuilding Qt 6.12 for QNX in a second container               | hours of build and more than the 11 GiB of free host disk allows, with the floor at 8 GiB                                                                                                                               |
| Published runner                                               | `gh release list` for the repository is empty, so there is no downloaded runner with a published hash to run                                                                                                            |

A build from the other direction (copying old binaries to the board and calling them evidence for
`main`) would not be evidence for these gates, so none was made.

## What ran on the Pi 400

| Step                                                                              | Result                                                                                                                                                                                                                                                                                                           |
| --------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Board reachable, `/dev/io-hid/io-hid` present                                     | yes ([board/baseline.txt](board/baseline.txt))                                                                                                                                                                                                                                                                   |
| Board screenshot with `sudo -n screenshot`, copied back                           | works ([board/baseline.png](board/baseline.png)). HDMI is black on the physical output, the capture is of the Screen framebuffer                                                                                                                                                                                 |
| Launched the installed tile app by hand, then stopped it                          | `rnqnx-tile-1/org.rnqnx.showcase`, SHA-256 `5e95f02259cbd72f55420e016daadca69f194066110227b25002b41756235a08`, file date 2026-11-13. It is not a build of current `main`; its revision was not recorded. Stopped with `--stop-running`; `pidin` showed no copy afterwards ([board/launch.txt](board/launch.txt)) |
| Screenshot of that app ([board/showcase-default.png](board/showcase-default.png)) | default title bar with macOS control layout, icon and `QNX Components` title, a task-switcher style popup showing the icon and name                                                                                                                                                                              |

The two screenshots are historical context for the board. They are not receipts for any gate:
the app predates the runtime layout switch, the title-bar inset, the S2 JavaScript thread and the
app-identity pipeline now on `main`.

One observation from `baseline.png`: the `QNX Components` tile label runs to both edges of its
tile, and the icon sits on a square dark background. That is the situation the `app-identity`
label-fit rule exists to prevent. It was not measured, and it comes from a build that predates
the rule, so it is a pointer for the device pass, not a finding.

## Remaining gap, per gate

### 1. `window-chrome`

Not met, in this order of size:

- The Nitro v1 windows API (`QnxWindows`: title, size, position, minimize, close, close guards,
  focus and visibility events, multi-window scope statement) is not implemented. The contract
  in `docs/nitro/API_CONTRACTS.md` is revision 3 and the S8 receipt records that it still needs a
  third architect read before any spec or C++ is written.
- The inset reaching JavaScript and `SafeAreaView` has no test or receipt, because there is no
  JavaScript surface for it yet.
- Per-target receipts for `QWindow::startSystemMove` and `startSystemResize` (supported or
  unsupported) do not exist for the Pi 400 QPA.
- Device receipts missing: both layouts on a build with the runtime switch, minimize and restore
  with the `AppState` transition logged, a close guard that cancels and one that proceeds,
  chrome operable while the JavaScript thread is blocked on the Pi 400 (the VM run in
  `s2-vm-2026-11-21` was not the board).
- Needs: the windows API and its host tests first, then a cross-build in `qnx805-build`, then a
  Pi 400 pass.

### 2. `app-identity`

Host and QNX VM items are recorded in `app-identity-host-vm-2026-11-21`. Still open:

- Custom icon on the launcher tile, title bar, task switcher and `QGuiApplication::setWindowIcon`
  on a device, from a build that carries the generated icon set.
- The measured label box (width, font, size, line count) for the launcher on the board, and a
  tile grid on the board where a maximum-length name and a short name give identical tile sizes.
  The repository owns no launcher QML, so the label box on the board comes from the running
  `demolauncher`.
- Needs: a cross-build of an app prepared with `prepare-native` from current `main`, installed as
  a tile, then screenshots of each location.

### 3. `performance`

Not met. The timing probe has no QNX reader (`RNQNX_ENABLE_TIMING_PROBE` is rejected by CMake on
QNX), so the frame-swap hook and the RSS reader still have to be written, built for QNX and run.
All Pi 400 fields in `docs/releases/performance-budgets.json` stay `unmeasured` with their stated
reason. No number was taken from the historical app: without the probe it would not be a
startup-to-first-frame or presented-frame measurement, and the gate asks for revision and sample
sizes. Writing QNX-only code without a compiler and a device to run it on would be a guess.

- Needs: the reader (QNX frame-swap connection and an RSS source checked against the SDP headers),
  a QNX cross-build with the probe on, the Hello World, 1000-row list and drawer-drag bundles,
  and at least 20 runs per workload on the board. Drawer drag is also blocked on the host by
  `apps/performance-lab/fixtures.js` (gesture and Worklets path not qualified).

### 4. `expo`

Not met. Pull request #220 covers the hardware-free parts of this gate and was not touched. Open
here and not satisfiable today:

- A prebuilt development runner for the QNX board with a published hash does not exist (no
  release), so "run it on the device in the downloaded runner with Fast Refresh" cannot start.
- `apps/expo-router-example` has not run on the Pi 400, and no standalone package was built and
  launched with the SDP.
- Needs: a runner release with a hash, a Metro session from a host to the Pi 400 over the
  network, and the standalone build in `qnx805-build`.

### 5. `device-parity`

Not met, and not reachable from a Pi 400 at all. The acceptance text asks for results on a **stock
Pi 4 QSTI** and says Pi 400 results stay separate. Pi 400 results can be recorded after the
components are built for the device, but they will not flip this gate.

## Cleanup

| Item                                 | State                                                                                                                     |
| ------------------------------------ | ------------------------------------------------------------------------------------------------------------------------- |
| App on the board                     | stopped, no copy in `pidin`                                                                                               |
| `drm-rpi4`, `screen`, `rnqnx-winmgr` | untouched, still running                                                                                                  |
| `/system/etc/startup`, boot config   | untouched                                                                                                                 |
| Files left on the board              | `/tmp/gates-baseline.bmp`, `/tmp/gates-showcase.bmp`, `/tmp/gates-after.bmp`, `/tmp/gates-launch.log` (cleared at reboot) |
| Containers, caches                   | none started, nothing installed on the host, so no cache cleanup was needed                                               |
