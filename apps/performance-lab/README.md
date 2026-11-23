# Run the QNX performance workloads

This source-checkout fixture supplies two real React Native workloads, not
benchmark numbers. It uses the existing Fabric, Hermes, Yoga and Qt Quick
runtime. It has no package publication, native-module, network or premium
component dependency.

`hello-world.js` imports the existing minimal View/Text baseline unchanged.
`index.js` renders 1,000 deterministic, fixed-height documents through the
existing FlatList implementation. The list has no simulated scrolling or
JavaScript FPS counter. All data is synthetic and fixed across runs.

The third release workload, `drawer-drag.js`, renders the shipped `JsDrawer`
(a `PanResponder` edge strip and an `Animated.Value` that follows the pointer).
No script sets the drawer position: the timing probe posts a left-edge drag and
the drag back as pointer events, or `--timing-observe-ms` records pointer events
a person or `rnqnx-inject` sends through Screen. The responder system decides
whether the events are a drag. A run where the drawer did not move is a run
without a drawer-drag result, so check the log or a board screenshot.

## Build the list bundle

From the repository root, install the documented Node/pnpm versions and frozen
dependencies, then build the list:

```bash
pnpm install --frozen-lockfile
pnpm qnx bundle-qnx --app performance-lab --out build/performance-lab/list-1000.bundle.js
```

## Build the unchanged Hello World baseline

From the repository root:

```bash
mkdir -p build/performance-lab
cd apps/performance-lab
../../node_modules/.bin/metro build hello-world.js --config metro.config.js --platform qnx --dev false --max-workers 2 --out ../../build/performance-lab/hello-world.bundle.js
cd ../..
```

## Build the drawer bundle

`drawer-drag.js` needs the public `react-native` entry (the drawer imports it),
so it has its own Metro config:

```bash
cd apps/performance-lab
../../node_modules/.bin/metro build drawer-drag.js --config metro.drawer.config.js --platform qnx --dev false --max-workers 2 --out ../../build/performance-lab/drawer-drag.bundle.js
cd ../..
```

Run it with `--timing-json <file> --timing-drag-steps 20` for a posted edge
drag and the drag back, or with `--timing-observe-ms 8000` and drive the
window with `rnqnx-inject drag` from a second shell.

Both entries register the existing `RNQnxPlayground` surface. This directory is
an intentional source fixture rather than a separate workspace package;
dependencies come from the existing pinned root graph. The Metro configuration
resolves Babel helpers from the installed RN package without a new version pin.

## Run on the existing development host

Build the host renderer with the repository's
[toolchain prerequisites](../../docs/toolchain.md) and `linux-host-renderer`
preset. Then launch one workload:

```bash
./build/linux-host-renderer/bin/rnqnx_playground_host --bundle build/performance-lab/list-1000.bundle.js
```

For an offscreen render-and-exit smoke check:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software \
  ./build/linux-host-renderer/bin/rnqnx_playground_host \
  --bundle build/performance-lab/list-1000.bundle.js \
  --screenshot build/performance-lab/list-host.png
```

A valid screenshot establishes only the captured host scenario. It does not
measure startup-to-first-frame, display presentation, physical input, FPS or
resident memory. Preserve the command, exit status, log and source/bundle hashes
when attaching a result.

## Collect QNX measurements separately

Use the named board's existing runbook to build and deploy the full runtime:
[Pi 400](../../docs/getting-started/pi400-hello-world.md) or
[stock QSTI](../../docs/getting-started/raspberry-pi-today.md). Those are different
qualification lanes. This fixture neither provisions Qt nor changes boot or
runtime flags. Input remains subject to the platform's explicit experimental
configuration and actual device verification.

For each workload, keep board/image, build and runtime versions, source and
bundle hashes, display mode, measured refresh, raw traces, sample count,
warmups and collection limitations. Record startup-to-first-frame, presented
frame intervals and RSS with real native instruments. Application-launch wall
time, queued frames, screenshots and JavaScript callbacks are not substitutes.
Do not copy unit-test values into a hardware report.

The [frame-pacing plan](../../docs/platform-modernization/03-frame-pacing.md)
and [v1 acceptance record](../../docs/releases/v1-readiness.md) define the
remaining measurements. The separately reviewed
[evidence reporter](https://github.com/mikevocalz/react-native-qnx/pull/76)
can validate submitted captures; it does not collect them or promote a gate.

## Record timing on the host

Configure the host renderer with `-DRNQNX_ENABLE_TIMING_PROBE=ON` (default OFF;
Linux and macOS only). The playground host then accepts `--timing-json`, which
records startup to first frame, frame-swap instants and RSS, writes a new JSON
file and exits. The contract is in
[API_CONTRACTS.md](../../docs/nitro/API_CONTRACTS.md).

```bash
./build/host-budgets/bin/rnqnx_playground_host --bundle build/performance-lab/hello-world.bundle.js \
  --timing-json build/performance-lab/hello-world-1.json
./build/host-budgets/bin/rnqnx_playground_host --bundle build/performance-lab/list-1000.bundle.js \
  --timing-json build/performance-lab/list-1000-1.json \
  --timing-wheel-notches 40 --timing-wheel-gap-ms 100
node scripts/validate-budgets.mjs --check-run build/performance-lab
```

The budgets are in [performance-budgets.json](../../docs/releases/performance-budgets.json)
and the `performance-budgets` workflow runs this on a hosted runner. The result
is a host number. The probe has no QNX reader, so the QNX VM, Pi 400 and release
board fields stay `unmeasured` until S9.

## Time one remote image load in stages

`image-probe.js` loads one image through `Image.prefetch` (fetch, decode and the
completion reaching JavaScript), `Image.getSize` on the cached image (one round
trip with no fetch and no decode) and an `<Image onLoad>` on a fresh URL. A
heartbeat timer records how late the JavaScript task queue drains during each
stage. It logs `IMAGE_PROBE` lines with a monotonic clock and `Date.now()`
together. The server logs when each request began and how long it took to send.

```bash
node scripts/image-probe-server.mjs --file apps/performance-lab/assets/probe-640x427.jpg &
(cd apps/performance-lab && ../../node_modules/.bin/metro build image-probe.js --config metro.config.js \
  --platform qnx --dev false --max-workers 2 --out ../../build/performance-lab/image-probe.bundle.js)
./build/host-budgets/bin/rnqnx_playground_host --bundle build/performance-lab/image-probe.bundle.js \
  --timing-json build/performance-lab/image-probe-run.json --timing-settle-ms 15000
```

On a board, set `url` in `image-probe.config.json` to a server the board can
reach, rebuild the bundle and run it with the board's own launcher. The receipt
for what the host run showed is `docs/evidence/s4-budgets-2026-11-20/`.

## Check the fixture

```bash
node tests/js/qnx-performance-lab.test.mjs
```

The test checks deterministic data and the drawer workload entry, then builds
**both** entries with the installed Metro CLI and prints their SHA-256 hashes.
Each bundle subprocess has a 90-second limit; temporary outputs are removed.
The same test is automatically included by `pnpm test:js`. This is host
JavaScript packaging evidence only. Native rendering and physical measurements
remain separate tests, not implicit passes.
