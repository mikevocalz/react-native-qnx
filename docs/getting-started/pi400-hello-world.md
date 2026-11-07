# Minimal React Native Hello World

`apps/rn-playground/hello-world.js` is a separate static entry for the existing
Qt Quick/Fabric playground runner. It registers the existing `RNQnxPlayground`
surface and renders a centered white `Hello World!` on a dark background.
It leaves the default playground, native window dimensions, Metro platform,
renderer, and dependency versions unchanged.

## Build the QNX JavaScript bundle

From the repository root:

```sh
pnpm install --frozen-lockfile
mkdir -p build/hello-world
(
  cd apps/rn-playground
  ../../node_modules/.bin/metro build hello-world.js --platform qnx --dev false \
    --out ../../build/hello-world/hello-world.bundle.js
)
```

For the QNX lean VM, also compile this bundle using the matching host compiler:

```sh
: "${RNQNX_HOST_HERMESC:?Set this to the matching host hermesc executable}"
"$RNQNX_HOST_HERMESC" -O -emit-binary \
  -out build/hello-world/hello-world.hbc build/hello-world/hello-world.bundle.js
```

Both bundle formats were prepared; compiling bytecode does not execute React
Native. The existing native playground expects `RNQnxPlayground`. Select this app by
passing the generated file explicitly with `--bundle`; replacing the default
playground entry is unnecessary.

## Physical QNX execution prerequisites

The JavaScript bundle alone is not executable on QNX. The actual application
needs the existing React Native/Fabric/Hermes/Qt Quick native host, matching
QNX AArch64 libraries, a working QPA/Screen configuration, and the deployed
bundle. The current QNX CMake graph exposes the geometry-only
`rnqnx_react_qnx_probe`; the existing visual runner is now also exposed as the opt-in
`rnqnx_playground_qnx` target. Its source and 640×480 window are shared with
`rnqnx_playground_host`; the QNX visual target has not yet completed its
cross-build or physical rendering check.

The repository's [ADR-0001](../adr/0001-qt-quick-backend.md) requires a documented
licensing determination before acquiring, building, or deploying Qt for QNX.
The current native graph requires Qt 6.11.1 or newer, including Core, Gui, Qml,
Quick, and Network. Do not replace it with an older package or a different
renderer to skip that gate.

## Recorded preparation

On 2026-11-06, the entry bundled successfully for `qnx` with the locked
React Native 0.88.1 / React 19.3.0 / Metro 0.87.1 graph. Dependency/source
validation, TypeScript, repository JavaScript tests, and entry formatting
passed. An independent source review found no renderer or window-contract
change.

Separately, the existing `rnqnx_g0_cxx20_probe` and direct-Screen
`rnqnx_playground` cross-built with QNX SDP 8.0.5. Those are prerequisite
binaries, not execution of this React Native entry. After reconnecting the Pi,
the new C++20 probe executed and reported
`RNQNX_G0_CXX20_OK ranges=12 designated=21 thread=34 jthread=unsupported`.
The Hello World bundle was copied to the device, but has not been evaluated by
a native React Native host there. The Screen probe has not run successfully
on the device. The seven available macOS CTests passed; they do not include a
full Hermes/React Native application rendering pass.

The pinned Hermes `hermes-v260318099.0.5` lean library and JSI cross-built
with QNX SDP 8.0.5. The physical `rnqnx_hermes_qnx_probe` then passed as
`qnxuser`, including main-thread execution and worker stacks of 256 KiB and
1 MiB. See [the captured output](../evidence/pi400-hermes-2026-11-06.txt).

This required three corrections: excluding the unavailable `madvise` call
from the QNX branch, replacing guessed stack bounds with QNX thread-status
bounds, and defining `_QNX_SOURCE` in the canonical Pi toolchain. Without
that feature macro, strict C++ headers selected a smaller `pthread_attr_t`
layout than target libc, and `pthread_attr_init` overwrote the stack canary.
The native worker-stack checks exercise that failure as well as runtime
initialization. Native stack protection remains enabled.

The lean VM accepts bytecode, not JavaScript source. With the pinned host
compiler selected by `scripts/configure-hermes-qnx.sh`, compile the probe:

```sh
: "${RNQNX_HOST_HERMESC:?Set this to the matching host hermesc executable}"
"$RNQNX_HOST_HERMESC" -O -emit-binary \
  -out build/qnx-aarch64-debug/bin/bytecode-smoke.hbc \
  vnext/probes/hermes/bytecode-smoke.js
```

Deploy the probe, `libhermesvmlean.so`, and `bytecode-smoke.hbc` together, then
run in that target directory:

```sh
LD_LIBRARY_PATH="$PWD:/usr/lib" ./rnqnx_hermes_qnx_probe bytecode-smoke.hbc
```

Require exit status zero, both `RNQNX_HERMES_QNX_STACK_OK` lines, and the final
`RNQNX_HERMES_OK bytecode` marker. The script checks arithmetic, ES6 constructs,
async-arrow execution, and a call into the native JSI host function. This is
engine/JSI evidence; it does not run React Native or render this Hello World.
The existing host probe still runs its source-based checks without arguments.

The packaged graphics server rejected `RaspberryPi400`. An isolated official
legacy Pi 4 graphics stack created the DRM and Screen devices, detected HDMI2,
and rendered the vendor gears demo using Broadcom V3D 4.2. A Screen screenshot
and the user's physical HDMI observation confirmed that output. The Ethernet
link dropped repeatedly; the graphics startup was subsequently saved, but the following cold boot
returned neither Ethernet nor HDMI signal. A recovery boot file restored
the original startup and disabled the graphics compatibility attempt. Physical
Ethernet, qconn, and SSH access were then verified again. Automatic graphics startup remains disabled in that recovery boot. A later
controlled manual DRM/Screen/gears run remained remotely reachable while the
Hermes tests ran. This does not establish reliable graphics startup on cold
boot. The gears demo is not this React Native application.

**Status: bundle prepared; physical React Native rendering not yet verified.**
The temporary Qt evaluation is active as recorded below. The matching target
Qt/native host and physical rendering remain outstanding.

## Temporary Qt evaluation

The owner requested and activated a Qt for Device Creation Enterprise evaluation.
The account displays an expiry of 17 November 2026. Qt 6.12.0 LTS source has
been acquired through that account; target compilation and a rendered Qt Quick
window remain pending. See ADR-0001 for the restricted evaluation scope.

## Opt-in QNX visual runner

With the licensed matching Qt QNX build available, configure the existing QNX
preset with `RNQNX_BUILD_HERMES_QNX=ON`, `RNQNX_BUILD_REACT_QNX=ON`,
`RNQNX_FETCH_REACT_NATIVE_DEPS=ON`, and `RNQNX_BUILD_PLAYGROUND_QNX=ON`.
Set `CMAKE_PREFIX_PATH` to the matching target Qt SDK and build
`rnqnx_playground_qnx`. The new flag defaults OFF and rejects host builds or
a missing React target graph. Both negative cases were checked.

Deploy the actual Qt platform/QML plugins and shared-library dependency closure,
including `QtQuick.Shapes`, Qml Models/WorkerScript, PNG support and fonts. The
unchanged View factory imports Shapes even for this small app. Run the visual
target with `--bundle hello-world.hbc`; its file-loading path preserves binary
bytecode. The target build, dependency closure and rendered image remain pending.

CMake's QCC MOC predefinitions command omits normal compiler flags and its generic
compiler-target setting. The canonical toolchain now repeats the locked ARM
selector and `_QNX_SOURCE` in `CMAKE_CXX_COMPILER_ARG1`, which that command does
include. The generated Qt Core predefinitions command was checked, Qt Core
compilation passed that previously failing step, and a fresh C++20 target plus
the existing Hermes bytecode checks passed on the recovered physical Pi.

A subsequent minimal automatic graphics experiment also failed a reboot/power
cycle test (black/no HDMI and no physical Ethernet link). The exact saved
recovery boot config was restored from the Mac; the recovery image checksum was
verified and system/data partitions were untouched. Network access and manual
DRM/Screen/gears then returned, with both physical probes passing again.
Automatic graphics remains disabled. The stock startup script's conditional
desktop-client launch is a source-backed difference from the successful manual
path, not a proven explanation of the boot failure; no helper log was created
by the failed boot.

For the preserved 640×480 window, select `QT_QPA_PLATFORM=qnx:no-fullscreen`.
Qt 6.12.0's `qqnxintegration.cpp` enables `FullScreenApplication` unless that
parameter is present; this environment setting preserves the existing window
contract rather than changing the runner's dimensions.
