# CURRENT_STATE.md

## 2026-11-07 real React Pressable remount recovery

Synthetic Qt input now exercises the real production-bundled React Pressable
and responder path on Pi 400/QNX 8.0.5. The probe caught a stale responder after
unmount: RN's event path skipped cancellation when the native target was gone.
An exact, opt-in RN patch preserves nullable cancellation and error forwarding.
The same executable/HBC passes with the fix and reproduces the remount failure
when only that fix is disabled. Delivery interception also fails as expected;
a subsequent normal run succeeds. Readbacks matched both artifact hashes.

Fresh host graphs passed 23/23 with input/tracing ON and 16/16 with both OFF.
The existing input option stays OFF by default. The continuous Hello World app,
recovery boot and window contract are preserved. This is synthetic input, not
physical HID, RNGH or Worklets qualification. See the
[responder receipt](../evidence/pressable-qnx-2026-11-07.md).

## 2026-11-07 bounded native tracing

The default-OFF trace experiment now records actual host evaluation, surface
start/stop and owned queue admission/execution/close with bounded storage,
session-relative monotonic timestamps and explicit drops. Physical Pi 400
application export, protected-file failure, OFF-path capture and traced Nitro
worker/shutdown assertions passed. Host graphs passed 20/20 ON and 18/18 OFF,
followed by 4/4 focused tests after adding queue correlation assertions. See the
[trace receipt](../evidence/pi400-runtime-trace-2026-11-07.md).

These are native boundary diagnostics. Input/Fabric/render-loop/presentation
markers, clock alignment, tracing overhead and performance budgets remain open.
No scheduling, boot, graphics or default-window policy changed.

## 2026-11-07 bounded application runtime trials

The screenshot runner now fails on real React startup or surface-cleanup errors
before publishing an image. A direct-child native watchdog and repeated host
harness preserve logs and distinguish error, crash, deadline and interruption.
The physical Pi 400 candidate completed ten fullscreen capture/Nitro/exit trials;
startup/cleanup failure controls, deadline, crash and a subsequent successful
launch also ran. The original continuous display app and recovery boot were
preserved. See the [scoped receipt](../evidence/pi400-runtime-baseline-2026-11-07.md)
and [operator instructions](../validation/runtime-baseline.md).

This is an implemented application-testing slice of Plan 01. The rest of the
delivery plans remain work, including cold-boot graphics recovery, physical
input/gestures, text/IME, services, packaging and measured performance. Captured
images are not proof of new HDMI presentation or startup latency. Current host
native validation is 16/16; older counts below remain dated evidence.

## 2026-11-07 opt-in Qt/Nitro async runtime follow-up

The pinned alpha 105 Qt dispatcher now passes physical Pi 400/QNX 8.0.5 callback,
typed/void Promise, error, checkpoint, drain and teardown-race tests as qnxuser.
Host suite and source-patch regression results are in the
[receipt](../evidence/pi400-nitro-qt-2026-11-07.md). The optional dispatcher
flag remains OFF by default. A separate candidate retained the exact title and
1920×1080 fullscreen render; original live app PID 9433114 and recovery boot were
left intact. Forge still needs matching typed bindings and worker-disposal
integration; this does not make alpha 120 ABI-compatible or prove third-party
modules, reload or suspend/resume. Hosted CI remains blocked by GitHub billing.

## 2026-11-07 physical Pi 400 Hello World

The physical Pi 400 running the QNX 8.0.5 target now renders the production
React Native/Fabric/Hermes/Qt Quick entry as `qnxuser`. The app displays
`React Native QNX: Hello World`. Default mode remains 640×480; the owner
subsequently requested fullscreen, now verified and running at 1920×1080. The same
entry exercises the existing synchronous Nitro v1 alpha 105 HybridClass
(getters and add(20, 22)=42); screenshot run exited zero and the continuous
app was then launched for HDMI. See
[device evidence](../evidence/pi400-hello-world-2026-11-07.txt) and
[bring-up instructions](../getting-started/pi400-hello-world.md).

The selected Qt 6.12.0 target runtime was built under the private evaluation
through 17 November 2026. Manual graphics requires the isolated legacy Pi 4
DRM/Screen stack and named DRM ACLs. Recovery boot remains selected; automatic
graphics startup remains disabled after failed boot tests. Full component,
QSTI, async Nitro, performance, and production-packaging gates remain separate.
Follow-up: the full host/Hermes graph now passes 11/11 CTests, including the
viewport and original error-forwarding regressions. Host JSI uses one shared
library to preserve exception identity. Cached pointer membership removes the
all-registry scan; host mutation timings and remaining flat-sibling overhead
are in [the follow-up report](../evidence/pi400-host-validation-2026-11-07.md).
The optimized executable was independently captured on the Pi before promotion.

Earlier no-QNX-pixel statements below describe older baselines and are
superseded only for this measured Hello World slice.

## 2026-11-07 Forge native target follow-up (PR42)

The complete seven-engine Rust archive and QNX native owner now cross-build with
licensed SDP8.0.5 and Rust1.99.1 source-built std. Physical Pi400 probes as qnxuser
passed entropy/thread, separate Tally write/reopen, 500-task serial queue and owner
lifecycle checks. Exact patch/toolchain/hashes and limitations are in
[the receipt](../evidence/forge-qnx-2026-11-07.md). The opt-in getrandom0.3.4
patch preserves all engines and existing entropy/error handling. Native flags
remain default OFF; no app-facing JS binding or cross-alpha Nitro compatibility
is claimed. Hosted jobs are blocked before startup by GitHub billing/spending
limits. The existing fullscreen app/graphics/recovery boot were left intact.

## 2026-10-03 dependency security update

The earlier clean-audit statements below describe the 2026-10-01 baseline.
GHSA-vfj7-8cjw-p6xm subsequently flags transitive braces 3.0.3. This change
backports the upstream depth guard and adds an installed-source-verified audit
gate; the raw registry finding remains visible and is labeled locally mitigated,
not registry-clean. See `docs/security/braces.md`. QNX target evidence is unchanged.
The focused change includes the patch, pnpm configuration/lockfile, audit and
regression scripts, and this handoff. Remote CI for the proposed commit is pending.

## 2026-10-04 bundled React probe readiness

The QNX probe now has a shared host-testable production Metro fixture and checks
real React initial mount, state update, Fabric/Yoga geometry, and unmount. It
reports geometry-only evidence with an explicit host/QNX label. Source checks,
production Metro bundling and basic host checks pass. The real positive and four
negative probe runners pass with companion host-renderer PR #28 commit `24ba958`;
that graph is a prerequisite. Actual combined CMake/CTest passed 11/11, including
45 host-runtime cases and the two probe tests. No QNX cross-build,
pixel, or device claim is made. See [probe instructions](../react-render-probe.md).

## 2026-10-04 image-loader implementation (host only)

Added real asynchronous image fetching/header handling, bounded decoded memory
prefetch/query and shared Qt Quick provider reuse. The exact Qt 6.11.1 Linux
loopback HTTP/cache/provider test passed locally; the QnxImageLoader translation
unit compiles against the pinned RN 0.88 headers. The existing Image.getSize and new Hermes Promise contract cases also pass
locally under ASan, using the full-host graph build repairs from draft PR #28.
The image adapter/backend/provider/factory and host allocation site were rebuilt
together for that targeted integration run; this is not a full-suite pass.
See `docs/image-loader.md` for commands, contract and limits. The dedicated
image-loader workflow runs real local HTTP tests, without external services.

QNX SDP/Pi execution and Qt Network size/build-time deltas remain pending.
The existing SDK58 audit finding is separate and tracked by draft PR #27;
this image change does not bypass or downgrade that audit gate.

## Outcome

`main` now contains the Raspberry Pi/QSTI install lane and the Expo SDK 58
migration. The exact merged SDK-58 tree passed the permanent GitHub Actions
validation on 2026-10-01: frozen install, Prettier, dependency/source contracts,
TypeScript 7, peer checks, a production Metro bundle for `--platform qnx`,
repository architecture/codegen checks, Linux C++ host tests with CMake 3.31.10,
and the dependency security audit. The Pi shell tooling also passes Bash syntax
and ShellCheck on `main`.

QNX target proof is deliberately split by generation. Existing QNX SDP 8.0.2
cross-build and x86_64 VM/Screen results remain valid historical evidence from
the pre-SDK58 line. The current physical target baseline is QNX SDP 8.0.5 with
QSTI 0.5.0 build 50 on Raspberry Pi 4 or Raspberry Pi 5. That physical Pi run and the full React Native + Hermes + Qt Quick runtime on
QNX remain to be proven. Current SDP 8.0.5 cross-build tooling, the RN 0.88
Hermes QNX source rebase, synchronous Nitro QNX runtime, Expo Modules QNX JSI
bootstrap, and Expo QNX autolinking are now implemented; they still require
licensed-toolchain/physical-target execution before being promoted to QNX
runtime proof.

## Current baseline

- Direct runtime/tooling line: Expo 58.0.0 / SDK 58, React Native
  0.88.0-rc.3, React 19.3.0, Metro 0.87.1,
  `@react-native/metro-config` 0.88.0-rc.3, `babel-preset-expo` 58.0.4,
  Nitro Modules/Nitrogen 0.37.1, TypeScript 7.0.2, Node 26+, and pnpm 12.8.1.
- Resolved Expo internals: `expo-modules-core` 58.0.9,
  `@expo/metro` 58.0.0-rc.0, and `@expo/metro-config` 58.0.6.
- Hermes identity is intentionally split: `hermes-compiler` 260318099.0.4
  and bundled source tag `hermes-v260318099.0.2`.
- The dependency audit is clean at the configured moderate threshold. Scoped
  overrides pin patched xmldom, js-yaml, brace-expansion, and uuid versions,
  and `validate:dependencies` checks those resolved contracts.
- Current physical target baseline: QNX SDP 8.0.5, QSTI 0.5.0 build 50,
  Raspberry Pi 4/5, stock target identity `qnxuser@qnxpi.local`.
- Historical QNX proof: QNX SDP 8.0.2 on Windows x86-64 cross-built AArch64
  Screen binaries and ran the C++20 and Screen probes in a QNX x86_64 VM.
  Those results are not silently promoted to SDK-58/QNX-8.0.5 proof.
- The old RN 0.86 Hermes/QNX patch remains quarantined as historical work; the
  current RN 0.88 port uses the exact `hermes-v260318099.0.2` source and the
  current patch-preparation pipeline instead.

## Finished in the current working tree

- Expo SDK 58 / React Native 0.88 migration is merged with a regenerated
  pnpm 12 lockfile and current exact direct pins.
- Permanent `SDK 58 validation` CI passes on `main`, including the real
  production QNX Metro bundle, Linux C++ host smoke suite, and clean security
  audit.
- The Pi 4/5 same-day lane is merged: `scripts/pi-preflight.sh` validates
  AArch64le, Screen and SSH; `scripts/install-pi.sh` builds, atomically
  deploys, runs and captures stdout/slog2/screenshot evidence.
- The hardware install lane is hardened for repeatability: it performs three
  launch/exit cycles by default, requires every Screen init/clean shutdown
  marker, and automatically writes a verified SHA-256 evidence manifest.
- The RN 0.88 Hermes/QNX source rebase is merged against
  `hermes-v260318099.0.2`, including exact-source preparation, QNX
  cross-configuration, and import tooling.
- The synchronous Nitro 0.37.1 QNX runtime is merged with a
  `QnxPlatformInfo` HybridObject proof seam; async dispatch remains explicitly
  deferred to the real QNX JS event loop.
- The Expo Modules Core QNX JSI bootstrap and Expo QNX autolinking/config
  tooling are merged on the SDK 58 line.
- The current Pi runbook is
  `docs/getting-started/raspberry-pi-today.md`.
- The QNX SDP 8.0.2 Windows host toolchain is installed and produces AArch64 QNX
  binaries; `qcc -V`/`q++ -V` are captured in `artifacts/g0/`.
- The `qnx-pi4` and `qnx-aarch64-debug` presets configure and build successfully
  on the Windows PC, producing `rnqnx_g0_cxx20_probe` and `rnqnx_playground`
  linked against `libscreen.so.1` for AArch64 QNX.
- A QNX x86_64 VM boots under QEMU 11.1.0 on the same Windows PC with SSH on
  `root@127.0.0.1:2222`. `mkqnximage` required two Windows-specific workarounds:
  setting `HOME=/cygdrive/c/Users/Owner` for the license check, and manually
  resolving the `__LIB*` library placeholders that the SDP's `mkifs`/`mkqnx6fsimg`
  did not expand on Windows. The resulting IFS boots, mounts the system/data
  partitions, starts `sshd`, and accepts a deployed ED25519 key. Details and
  reproduction scripts are in `docs/handoff/QNX_VM_WINDOWS.md`.
- The full build-deploy-run loop was exercised for `rnqnx_g0_cxx20_probe`: built
  with the `qnx-x86_64-debug` preset, copied to the VM over SCP, and executed
  over SSH. It printed the expected
  `RNQNX_G0_CXX20_OK ranges=12 designated=21 thread=34 jthread=unsupported`
  result.
- The Screen-only Milestone 0 executable `rnqnx_playground` now runs on the
  same QNX x86_64 VM by switching the guest's Screen configuration from the
  missing virtio-drm GPU stack to a software/dummy WFD blitter. It initialized a
  1280x720 surface, rendered 600 alternating blue/black frames, and exited
  cleanly. This validates `QnxScreenSurface`, the QNX Screen lifecycle, and the
  cross-compiled link against `libscreen.so.1` end-to-end.
- A new repo script, `scripts/validate-qnx-vm.sh`, automates the full VM
  validation: it sources the QNX SDP environment, builds the G0 probe and
  `rnqnx_playground` for `qnx-x86_64-debug`, deploys and runs them on the local
  QEMU VM, and captures the outputs and round-trip timings in `artifacts/g0-vm/`.
- A new repo script, `scripts/audit-screen-symbols.sh`, verifies that every
  Screen function and constant used by `QnxScreenSurface` exists in the
  installed QNX `screen/screen.h` header. All 26 audited symbols are present in
  QNX SDP 8.0.2.
- The Screen executable `rnqnx_playground` also compiles and links for
  `qnx-aarch64-debug`; `ntoaarch64-readelf` reports `Machine: AArch64`. This
  confirms the Screen code and its QNX dependencies cross-build for the target
  architecture, even though a physical AArch64le device is still needed to run it.
- The workspace lockfile was regenerated with exact direct dependency pins.
- `validate:dependencies` verifies direct resolutions, peer baselines, the
  Hermes tag, resolved Expo internals, and named symbols in installed RN, Yoga,
  Nitro, and Expo source files.
- Every scaffold directory required by `validate:architecture` is tracked with a
  `.gitkeep` file so that validation survives a clean checkout.
- macOS/Linux host CMake presets build a strict-warning, ASan/UBSan test target.
- The production `QnxScreenSurface` lifecycle was corrected to use the typed
  window API, clean up partial initialization, encode ARGB fill attributes,
  preserve `errno`, and re-query the render-buffer list before each frame.
- Eight host cases exercise that concrete wrapper through a test-only subset of
  the documented QNX 8 Screen API, including all initialization failures and
  move ownership. This is host emulation, not an SDP compile or driver test.
- Validation, bootstrap, configure, build, deploy, architecture, manifest, and
  codegen-applicability scripts now fail explicitly instead of reporting
  placeholder success.
- The current SDK-58 dependency audit is clean at the configured moderate
  threshold. Historical dependency advisories are retained in `BLOCKERS.md`
  only as resolved context.

## Historical Qt Quick view layer evidence (pre-SDK58 baseline)

The detailed renderer evidence below was produced on the React Native
0.86 / Expo 57 host line. It remains useful architectural and regression
evidence, but the complete Qt + Hermes renderer path has not yet been rerun
under the SDK-58/RN-0.88 line. The current SDK-58 CI proves source contracts,
QNX Metro bundling and the Linux C++ host suite without claiming that optional
Qt/Hermes host targets were built on that runner.

`QtQuickItemFactory` implements ADR-0002's cached-`QQmlComponent` strategy
against installed Qt 6.11.1 on macOS, per ADR-0010. It compiles one component
per React Native component type from inline QML type source, instantiates items,
parents them, and reports compile/type failures instead of returning partially
constructed objects. `rnqnx_qtquick_host_probe` exercises and benchmarks it
headlessly in CTest.

`QtQuickMountingLayer` adds the platform half of the Fabric mounting layer over
that factory: create, insert, remove, destroy, and updateLayout, with an
operation set mirrored field-by-field from `ShadowViewMutation`. Fifteen host
cases cover ordering, detachment, ownership, and every failure path under
ASan/UBSan. Layout is applied only here, as absolute geometry, because Yoga is
the layout authority and no Qt layout or anchor participates.

`ShadowViewMutationAdapter` connects that layer to React Native. React Native's
renderer headers now compile here against fetched folly, fmt, glog,
double-conversion, and fast_float, pinned at the versions the installed package
itself pins. The adapter consumes real `ShadowViewMutation` values and reports
failures rather than skipping views; only layout is applied so far, which is an
honest gap rather than a silent partial update.

React Native's renderer itself builds here now: 41 modules, from Yoga and JSI
through mounting and the UIManager, compiled from the installed package using
React Native's own per-module CMake files.

The pipeline is now proven end to end on the host. `shadow_tree_mounting_host_test`
commits a real `ShadowTree`, lets React Native's differ compute the mutations,
pulls them through `MountingCoordinator`, and asserts Yoga's computed geometry —
fixed sizes, flex distribution, padding offsets, in-place resizes, and unmounts
— on the resulting Qt Quick items.

Hermes is the other half. `hermes-v250829098.0.16`, the version React Native
0.86.2 actually defaults to, builds from source on this host and runs JavaScript
through JSI, including the async-arrow case that a previous out-of-tree platform
found broken.

The two halves are now connected. `QtReactHost` builds React Native's
`Scheduler` on top of a Hermes runtime, supplying the pieces React Native has no
generic implementation of — an `EventBeat` driven by the Qt event loop, a
component registry factory, and a `SchedulerDelegate` that mounts from
`schedulerShouldRenderTransactions`. Surface commits flow through React Native's
own UIManager and land on Qt Quick items with Yoga's geometry.

JavaScript now drives the tree. A script calling
`nativeFabricUIManager.createNode`/`createChildSet`/`appendChildToSet`/
`completeRoot` — the surface React's own renderer drives — produces a Qt Quick
item whose geometry Yoga computed from props JavaScript supplied. The component
predicate JavaScript uses to resolve native components is installed too.

Metro bundles for `qnx`. `@rn-qnx/metro-config-qnx` registers the platform, and
`apps/rn-playground` produces a real bundle during the build carrying
`RN$AppRegistry`, `RN$stopSurface`, and its registered component. The host
installs the bridgeless globals React Native's JavaScript branches on before any
evaluation.

**A React Native bundle renders.** Metro bundles `apps/rn-playground` for
`qnx`, Hermes evaluates it, `AppRegistryBinding::startSurface` runs
`RN$AppRegistry.runApplication`, React reconciles, and the resulting mount
transaction lands as Qt Quick items with Yoga's geometry. That is the full path
from JSX to pixel geometry, on the development host.

What made it work, beyond the mounting layer and the runtime host: the
TurboModule adapter ADR-0004 sanctions (Nitro remains the mechanism for every
application-facing API); two JavaScript platform overrides in
`@rn-qnx/react-native-qnx` — `Platform.js` and `BaseViewConfig.js` — selected by
a Metro `resolveRequest`; Hermes' microtask queue, which is off by default;
`QTimer`-backed timers, because timers are a host responsibility rather than an
engine builtin; and platform constants read from the running system.

Background colour, corner radius, border, and opacity are applied too, so the
rendered tree is visible rather than merely correctly sized.
`docs/component-support.md` is the honest table of what works.

The result is verified on glass: `renderedTreeIsPainted` grabs a rendered frame
and reads pixels, and `rnqnx_playground_host` shows the same app in a real
window or writes a PNG.

Touch reaches JavaScript and React's responder system runs on top of it:
`QtTouchDispatcher` filters Qt pointer events, hit-tests to the deepest mounted
item, and calls its `TouchEventEmitter`; a press then asks
`onStartShouldSetResponder`, grants, and releases. The scheduler delegate
forwards the responder claim so the rest of a gesture follows the responder.
`Pressable` renders and its `onPress` fires from a real pointer press. One
pointer only.

`ScrollView` mounts as a Qt `Flickable`, with children routed into its
`contentItem` and its content size recomputed as they change, so it clips and
scrolls.

`ActivityIndicator`, `Switch`, `Modal`, `SafeAreaView`, `StatusBar`, and
`RefreshControl` all mount with their primary props. `Switch` toggles and
reports `onValueChange`; `Image` reports load lifecycle; `Modal` renders as a
full-screen overlay; `SafeAreaView` behaves as a `View`; `StatusBar` and
`RefreshControl` are no-ops on QNX. See `docs/component-support.md` for the
full table.

`Image` renders from `data:` URIs, decoded by this platform's image provider
because Qt Quick cannot read them, with `SourceCode` implemented as the
TurboModule the image path needs. `resizeMode` maps to QML `Image.fillMode`,
and `onLoadStart`, `onLoad`, `onLoadEnd`, and `onError` reach JavaScript.
`ImageBackground` composes `Image` and `View` with a QNX-specific override.

`Text` renders too, measured by this platform's own `TextLayoutManager` using
`QTextLayout` — React Native's generic C++ manager does not measure at all and
would have collapsed every string to nothing. Mixed styling within a paragraph
is carried as Qt rich text, long text wraps, and `numberOfLines` ellipses.

JavaScript errors are reported to the host rather than aborting the process:
`QnxExceptionsManager` implements React Native's ExceptionsManager over the
generated spec, and a healthy render is asserted to report nothing.

Remaining gaps: transforms and `onLayout` are unimplemented, per-corner radii
collapse to one value because Qt Quick's `Rectangle` has a single `radius`, and
only the left border edge is read for the same reason. Single-touch pointer
events reach JavaScript, but multi-touch, pointer capture, and gesture
recognisers are not implemented. Seven of the nine TurboModules the bundle
references are still unimplemented — the render does not touch them, but an
error path would reach ExceptionsManager immediately. The rendering surface is
the offscreen QPA plugin, not glass.

## Installed-source boundaries

- RN Fabric entry points are grounded in installed `SchedulerToolbox`,
  `SchedulerDelegate`, `MountingCoordinator`, and `UIManagerBinding` sources.
- Nitro's installed generic C++ entry point is `margelo::nitro::install`, and its
  registry entry point is
  `HybridObjectRegistry::registerHybridObjectConstructor`.
- Nitrogen generation and Nitro platform specs currently support only iOS and
  Android; QNX generation/autolinking has not been designed.
- Expo Modules Core installs `globalThis.expo` only through its Apple/Android
  implementations; there is no installed QNX runtime installer.
- Expo Metro defaults omit `qnx`; no QNX Metro configuration exists yet.
- The installed RN package identifies the Hermes tag but contains no QNX Hermes
  build recipe or full Hermes source checkout.

No RN/Fabric/Nitro/Expo native QNX seam should be implemented until its design is
cited to these installed source files and symbols.

## Verification scope

See `TEST_STATUS.md` for command results. On the current SDK-58 line, frozen
dependency installation, formatting, source contracts, TypeScript, peers,
production QNX Metro bundling, architecture/codegen checks, Linux C++ host
tests, and the dependency audit pass.

Separately, historical QNX SDP 8.0.2 evidence includes AArch64
cross-compilation plus successful x86_64 VM execution of the C++20 and Screen
probes. Current SDK-58 QNX cross-compilation, current QSTI physical-Pi execution,
and the full React Native + Hermes + Qt Quick runtime on QNX have not yet been
proven.

## Repository state

`main` is merged through the SDK-58, formatting, security-hardening, Pi tooling,
and permanent validation PRs. The latest known-good merged commit is
`47c3d668b0ba61eaf81ce8083b87c3bf82c876ce`, validated by main-branch
GitHub Actions runs 36896516617 and 36896516638.

Continue with `NEXT_ACTIONS.md` and `QNX_VALIDATION_QUEUE.md`. Do not mark
the current physical-target gate complete until the QSTI Pi run is captured,
and do not claim the full RN/Hermes/Qt QNX runtime until that stack is rebased,
cross-built and executed on the current QNX line.
