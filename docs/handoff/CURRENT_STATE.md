# CURRENT_STATE.md

## Outcome

The macOS-host Milestone 0 foundation is complete and reproducible. The QNX
Milestone 0 exit gate is not complete because this workspace has no licensed QNX
SDP, installed `screen/screen.h`, Linux cross-compiler, or runtime target.

## Current baseline

- Authoring mode: mac-first; QNX remains the production target.
- Direct runtime/tooling line: React Native 0.86.2, React 19.2.3, Expo 57.0.12,
  Metro 0.84.4, Nitro Modules/Nitrogen 0.36.5, TypeScript 6.0.3, and pnpm
  11.21.0.
- Resolved Expo internals: `expo-modules-core` 57.0.10, `@expo/metro` 56.0.0,
  and `@expo/metro-config` 57.0.8.
- A scoped `xcode>uuid` override replaces vulnerable uuid 7.0.3 with 11.1.1;
  the installed CommonJS `uuid.v4()` seam is validated.
- Hermes identity from installed React Native source:
  `hermes-v250829098.0.16` / `hermes-compiler` 250829098.0.16.
- Yoga is the source snapshot bundled in React Native 0.86.2; no independent
  Yoga version is asserted.

These are exact, mutually compatible pins for the current Expo 57 / React Native
0.86 platform line. They are not a claim that incompatible next release lines
such as React Native 0.87, Metro 0.87, or TypeScript 7 can be substituted.

## Finished in the current working tree

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
- The registry audit remains non-clean because no fixed `image-size` release is
  published for two Metro development-tool denial-of-service advisories. See
  `BLOCKERS.md`; Metro must not process untrusted assets.

## Qt Quick view layer (host)

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
`Pressable` renders and its `onPress` fires from a real pointer press, which is
the first component here that is not a bare `View`. One pointer only.

JavaScript errors are reported to the host rather than aborting the process:
`QnxExceptionsManager` implements React Native's ExceptionsManager over the
generated spec, and a healthy render is asserted to report nothing.

Remaining gaps: transforms and `onLayout` are unimplemented, per-corner radii
collapse to one value because Qt Quick's `Rectangle` has a single `radius`, and
only the left border edge is read for the same reason. There is no input pipeline, so nothing
responds to touch. Seven of the nine TurboModules the bundle references are
still unimplemented — the render does not touch them, but an error path would
reach ExceptionsManager immediately. The rendering surface is the offscreen QPA
plugin, not glass.

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

See `TEST_STATUS.md` for command results. Host format/type/source/architecture
checks and the sanitizer-backed C++ suite pass. No QNX translation unit has been
compiled against an installed SDP, and no QNX binary has run.

## Repository state

The working tree is clean and nothing is pushed; no git remote is configured
yet. Repository-local material that is not part of the published repository is
excluded through `.git/info/exclude` rather than a tracked ignore file. Continue with `NEXT_ACTIONS.md` and
`QNX_VALIDATION_QUEUE.md`; do not mark Milestone 0 exited until QNX-RN-008
passes.
