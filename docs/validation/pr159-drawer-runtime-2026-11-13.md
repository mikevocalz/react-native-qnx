# Drawer input and retained TurboModules

Base: `b969cd2`, PR 159. This continuation fixes three failures found while
running the actual gallery. It keeps the existing Fabric, Hermes and Qt Quick
backend and the established 1280x800 window.

## Behavior

The JavaScript drawer released a swipe by asking its controlled parent to open,
then immediately animating toward the old closed prop. It now settles toward
the requested state, accepts parent confirmation, and reverts after 400ms if
the parent ignores it. A rejected close restores panel content and input.
Cancellation, a new gesture and unmount cancel pending timers/animations.
Touched React state uses a per-instance Zustand store and selector subscriptions;
Animated values, timers and callbacks stay in refs. The router package now
declares the existing exact Zustand 5.1.0 peer and regenerates its lock importer.

The native drawer's older tests declared accessibility properties in QML.
Production mounting supplies dynamic QObject properties, which QML dot lookup
does not expose. The native bridge now reads those properties through QObject.
Real mounted items therefore participate in hover, disabled filtering, selected
item discovery and keyboard focus. Keyboard activation uses a per-item native
callback bound to the current React event emitter. It requires an accessible,
visible, enabled item with a click handler and a descendant of the drawer.
Updating/unmounting the view replaces/destroys the callback with its QObject.

RN 0.88.1 Pressability handles keyboard/accessibility clicks through onClick and
ignores native events containing pointerType to avoid double presses. The QNX
base view config omitted topClick/onClick transport. It now declares the same
standard click/capture entries as the inspected Android/iOS configs. The native
activation emits a discrete click with its React target and no pointerType;
pointer input keeps its existing touch path. This establishes navigation input,
not an operating-system screen-reader result.

A fresh optimized Metro/Hermes bundle also exposed stalled image prefetch after
cache clearing. Expo's C++ helper makes a temporary nativeModuleProxy lookup;
the host constructed a new ImageLoader for each lookup and did not retain it.
Garbage collection could destroy it and cancel a pending fetch. The host now
retains one successful TurboModule instance per name for its own lifetime,
including custom providers. Failed lookups remain uncached. Module destruction
still occurs on the owning thread while the runtime exists. Providers should
be configured before their first lookup.

## Source and validation

Inspected installed RN 0.88.1 TurboModuleBinding.cpp, TurboModule.h,
Pressability.js, PointerEvent.cpp, TouchEventEmitter.cpp and both standard base
view configs, alongside the Expo Image forwarding helper and current mount
paths. The source inventory records exact bytes. No dependency versions change
beyond declaring the already-pinned Zustand peer; frozen install and supply-chain
policy checks pass. Lock SHA-256:
`30ef88d1637b74f719c1e0e82fb0cc61b997b152542e26760186484890ad2a77`.

Host checks:

- Real React renderer settlement tests pass for delayed confirmation, ignored
  open/close requests, cancellation, unmount and rail updates. Old source fails
  because release animates to 0 when it requests 1.
- Router rendering, native drawer prop/event contract, tab options, standard
  click transport, typecheck and dependency validation pass.
- All 34 standalone Qt native-UI cases pass in Release and ASan/UBSan, including a new
  regression using actual dynamic QObject metadata. Before the fix its selected
  index is -1 instead of 0. Installed Qt remains uninstrumented and macOS leak
  detection is disabled. The standalone graph uses a recorder bridge for QML;
  real Fabric click routing is exercised by the Pi run.
- A real Hermes/Fabric host test forces GC after a temporary module lookup and
  checks one retained instance, one provider construction and release at host
  teardown. Removing retention fails `!retained.expired()`; restoration passes.
  This local transplanted Linux graph has two separate failures (outer painted
  pixel and a missing ModalHostView fixture mapping) in both configurations.
  It is a targeted lifetime regression result, not a green full-host suite.

Evidence is under `docs/evidence/drawer-runtime-2026-11-13/`. Exact-head hosted
checks and independent review are required after publication. The CodeRabbit
CLI quota currently prevents another review; no completed review is claimed
for this continuation.

## Pi 400 results and provenance

The QNX SDP 8.0.5 AArch64 build passes. The inherited consumer is Release/O1
with selected GCC warning exceptions; public-source Qt 6.12.0 is O2. Upstream
objects were reused through the previously disclosed launcher, while project
sources were compiled. A fresh final combined build without that launcher is
still required. These are custom-image Pi 400 results. Marathon, QNX VM and
stock Pi 4/5 QSTI are separate lanes.

The JavaScript proof card shows open after a 200ms parent confirmation, close
after a second swipe, and closed after an ignored third request. Screen virtual
pointer events drive the gestures. Native-menu captures show hover highlighting,
a Down-key focus ring on Components, and Enter navigating to Components and
closing the drawer after click transport was added. The retained-module binary
completes cache/ImageRef/blurhash APIs with the same optimized HBC that stalled
on the preceding binary. Capture names and artifact hashes identify each stage;
no input-to-presentation latency or frame-rate claim is made.

The frozen native consumer is
`e93f22ef0227a443453e7aa5688c22c1297e777af1c139ae5253169385d3e6ba`.
Final HBC:
`b03f946c76a78df2d9a195dadc9a469efb1fed42e62cbb3869b572f88d3f401b`.
The final pair repeats Enter navigation and the completed image API sequence
(`final-enter.png`, `final-apis.png`); Home then Enter returns to the gallery.
Nine selected native source files match the cross-build tree. One showcase
process remains and the USB input endpoint exists after testing. Later HBCs
were selected explicitly with `--bundle`; the earlier prepared descriptor is
not their packaging receipt.

Earlier Linux image QA bundles retained extra SVG gallery metadata from the
reused source directory. The fresh Mac Metro bundle used here comes from the
current checkout and contains ten Advanced cards. Image results from earlier
bundles remain bounded by their recorded HBC hashes. Final qualification must
start from a clean source tree.

## Ownership, skills and rollback

DRI: current integration agent, owning the router fallback, native UI bridge,
host module cache, transport config, regression tests and gallery driver.
Human review is unassigned. Read repository qnx-native-api and
qnx-platform-validation; the pinned Margelo api-design → build-nitro-modules →
cpp pipeline at `1e9e17be6f41c838db472d6a4b943fc26355a3be` kept API contracts,
QObject lifetime and event ownership explicit. No Nitro code generation or
registration change is needed. The pinned React Native guidance and no-ai-slop
rules informed state ownership and this receipt.

No new permission, premium payload, subscription requirement or SDK artifact is
introduced. The click callback admits only registered handlers and cannot name
an arbitrary native event. Generic callback allocation occurs only for eligible
views with native router UI enabled. Performance and allocation deltas are
unmeasured. ImageLoader's explicit cache operations and ImageRef decode/hash/
encode work still need worker conversion; the full router prop/RTL/resize
conformance work remains open.

Rollback: revert this continuation's source, test and peer/lock changes together,
retaining prior image-resize and fetch-worker commits. Rebuild native binary and
HBC, then use the guarded launcher. Keep the recorded historical captures and
their limits when updating the handoff.
