# Run the QNX performance workloads

This source-checkout fixture supplies two real React Native workloads, not
benchmark numbers. It uses the existing Fabric, Hermes, Yoga and Qt Quick
runtime. It has no package publication, native-module, network or premium
component dependency.

`hello-world.js` imports the existing minimal View/Text baseline unchanged.
`index.js` renders 1,000 deterministic, fixed-height documents through the
existing FlatList implementation. The list has no simulated scrolling or
JavaScript FPS counter. All data is synthetic and fixed across runs.

The third release workload, `drawer-drag`, is explicitly blocked in
`fixtures.js`. A static panel or scripted position change would not qualify
the missing physical gesture/Worklets path, so no substitute is included.

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

## Check the fixture

```bash
node tests/js/qnx-performance-lab.test.mjs
```

The test checks deterministic data and the blocked drawer state, then builds
**both** entries with the installed Metro CLI and prints their SHA-256 hashes.
Each bundle subprocess has a 90-second limit; temporary outputs are removed.
The same test is automatically included by `pnpm test:js`. This is host
JavaScript packaging evidence only. Native rendering and physical measurements
remain separate tests, not implicit passes.
