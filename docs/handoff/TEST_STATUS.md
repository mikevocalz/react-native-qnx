# TEST_STATUS.md

## Image renders — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- The playground renders an `<Image>` from a 2x2 PNG inlined as a `data:` URI,
  so nothing depends on the network or the filesystem. The test asserts the
  source became an `image://rnqnx/…` URL, the size is the styled 40x24, and the
  item reaches `Image.Ready` — which is what proves the bytes were decoded
  rather than merely assigned. The rendered frame contains exactly 960 magenta
  pixels, which is 40x24.
- Three things had to exist first, each found by running it:
  - `Image` is a fourth platform-resolved JavaScript file, after `Platform` and
    `BaseViewConfig`. Without an override the import is undefined and React
    throws "Element type is invalid".
  - `SourceCode` is required before an image can resolve, because
    `getDevServer` reads `scriptURL` from it. `QnxSourceCode` reports the bundle
    the host actually evaluated.
  - Qt Quick's `Image` does not read `data:` URIs — the item stays in `Loading`
    forever. `QnxImageProvider` decodes them and serves them back through an
    `image://` URL, which is Qt's supported mechanism.
- React Native's generic C++ `ImageManager` is a documented stub that does not
  fetch or decode. That is correct here: Qt loads the source itself, so the
  manager has nothing to do on this platform.
- Mutation check, build verified first: making the source resolver a pass-through
  fails the case, because the data URI never becomes a provider URL.
- Evidence level: host only. Nothing cross-compiled for QNX, nothing run on the
  Pi 4.

## Text wraps and mixes styles — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- Measurement moved from `QFontMetricsF` to `QTextLayout` with a format range
  per fragment, so a paragraph with mixed fonts measures correctly and long text
  wraps against the available width. React Native passes infinity for an
  unconstrained axis, which `QTextLine` cannot take, so that is guarded.
- Mixed styling is carried to the item as Qt rich text, because Qt Quick's
  `Text` applies one font and colour to the whole item. The plain string is kept
  for the single-style case so markup characters are never interpreted.
- The item is set to wrap as well. Without that the measurement said two lines
  while the paint said one, and the text ran outside its own frame — visible in
  the screenshot before the fix.
- Mutation checks, both with the build verified first: forcing the plain-text
  branch fails the rich-text assertion, and setting the measurer to `NoWrap`
  fails the wrapped-height assertion.
- **Method correction.** Two mutation runs this session reported a false pass
  because the mutated source did not compile and the old binary was still on
  disk. Mutation runs now check the build's exit status before the test's.
- Evidence level: host only. Nothing cross-compiled for QNX, nothing run on the
  Pi 4.

## Text renders — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- The playground renders `<Text style={{fontSize: 24}}>Hello QNX</Text>` and the
  test asserts the string reaches the Qt item, the font size is 24, and the
  measured size follows from the font rather than from an explicit style.
- The measurement is this platform's own `TextLayoutManager`. React Native's
  generic C++ one returns `layoutConstraints.minimumSize` — it does not measure
  — so every `<Text>` would have collapsed to nothing. Ours measures with
  `QFontMetricsF`, substituted the same way Android and Apple substitute theirs.
- Mutation check: changing the app's `fontSize` to 12 fails the case on the
  pixel size.
- Rendered and looked at: "Hello QNX" appears in black at 24px inside the blue
  view.
- A bug found by looking: the first screenshot showed no text at all, because
  `rnqnx_playground_host` registered only the `View` QML type and never mapped
  `Paragraph`. The test fixture had both, so the tests passed while the runner
  rendered nothing — a reminder that two entry points need the same setup.
- Evidence level: host only. Nothing cross-compiled for QNX, nothing run on the
  Pi 4.

## Pressable works — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- The playground renders a `Pressable`, and `pointerEventsReachJavaScript`
  asserts `onPress` fires from a real `QMouseEvent` press and release. This is
  the first component rendered here that is not a bare `View`.
- Mutation check: moving the press to a point outside the `Pressable` fails the
  case, so the assertion is about the hit, not merely about a press happening.
- This also settles the previous entry's open question. `Pressable` was never
  broken; the earlier run tested an app the edit had failed to reach.
  `docs/component-support.md` moves it from ❓ to ✅.
- `metroBundleRendersThroughReact` was updated to expect two children rather
  than one, which is how the mounted `Pressable` first announced itself.
- Evidence level: host only, offscreen QPA. Nothing cross-compiled for QNX,
  nothing run on the Pi 4.

## The JavaScript responder system works — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- `pointerEventsReachJavaScript` now also asserts React's responder
  negotiation: a press asks `onStartShouldSetResponder`, the view claims it,
  `onResponderGrant` runs, and the release delivers `onResponderRelease`. That
  machinery is what `Pressability` and every gesture are built on.
- `QtSchedulerDelegate::schedulerDidSetIsJSResponder` forwards the claim to the
  dispatcher, which then routes the rest of the gesture to the responder rather
  than to whatever is under the pointer.
- Mutation check: returning `false` from `onStartShouldSetResponder` fails the
  case on `__granted`.
- **`Pressable` is not claimed either way.** An attempt to render one never
  landed in the app — the scripted edit failed silently — so the run that
  appeared to show `Pressable` failing was really testing an app with no
  `Pressable` in it. `docs/component-support.md` marks it untested rather than
  broken. That is the third silently-dropped edit to produce a misleading
  result this session; every scripted edit now gets grepped back afterwards.
- Evidence level: host only, offscreen QPA. Nothing cross-compiled for QNX,
  nothing run on the Pi 4.

## Touch reaches JavaScript — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- `pointerEventsReachJavaScript` sends a `QMouseEvent` press and release to the
  window and asserts the playground's `onTouchStart` and `onTouchEnd` handlers
  each ran once in JavaScript, and that `event.nativeEvent.locationX` is 20 —
  the coordinate survived the trip from Qt into React Native's touch payload.
- Mutation check: removing the `onTouchStart` call fails the case on
  `globalThis.__touchStarts was never set`.
- Two bugs found and fixed, both mine:
  - The dispatcher was a local in the test while the adapter held a raw pointer
    to it. Stopping a surface commits an empty tree during host teardown, which
    reaches the adapter and then the freed dispatcher — heap corruption, an
    `EXC_BREAKPOINT` inside malloc rather than a clean failure. The fixture now
    owns it, declared before the host, and the lifetime rule is documented on
    the API.
  - The harness printed `[PASS]` to buffered stdout, so a crash in a later case
    discarded the record of which cases had passed and made the failure look
    like it happened first. It now flushes.
- Evidence level: host only, offscreen QPA. Nothing cross-compiled for QNX,
  nothing run on the Pi 4.

## The tree is painted, not just configured — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; sixteen host cases.
- `renderedTreeIsPainted` renders the playground bundle into a shown
  `QQuickWindow`, grabs the frame, and reads pixels back. Every other test
  asserts item properties, which only proves the mounting layer set them; this
  is the one that proves Qt Quick drew what they describe.
- Asserted: the outer view's centre is exactly `#204080`; a point outside it is
  the window's white backdrop, so the view is bounded rather than filling the
  surface; and the inner view's pixel sits between the two colours, which is the
  50% opacity being composited by the renderer rather than merely stored.
- `grabWindow` works under the offscreen platform plugin, so this runs headless.
- Mutation check: changing the app's colour to `#804020` fails on
  `outer.red() == 0x20`.
- `rnqnx_playground_host` renders the same app in a real window, or writes a PNG
  with `--screenshot`. The rendered frame shows a 320x180 blue rounded rectangle
  with a half-transparent orange child, matching the JSX.
- Evidence level: host only. Nothing cross-compiled for QNX, nothing run on the
  Pi 4.

## JavaScript errors reach the host — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; fifteen host cases.
- `QnxExceptionsManager` implements React Native's ExceptionsManager over the
  generated `NativeExceptionsManagerCxxSpec`. Before it existed, any JavaScript
  error aborted the process: React reached for the module while reporting,
  `getEnforcing` threw, and the throw escaped.
- `javaScriptErrorsReachTheHost` reports an error from JavaScript through
  `nativeModuleProxy` and asserts the host received the message, the fatal flag,
  and a flattened stack that still names the method and `App.js:42`.
- `aSuccessfulRenderReportsNoErrors` renders the playground bundle and asserts
  nothing was reported, which keeps the error path from quietly becoming noisy.
- Mutation check: dropping the `reporter_` call fails
  `javaScriptErrorsReachTheHost` on `errors.size() == 1`.
- Evidence level: host only, offscreen QPA. Nothing cross-compiled for QNX,
  nothing run on the Pi 4.

## Visual props reach the items — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; thirteen host cases.
- The playground app now declares `backgroundColor`, `borderRadius`, and
  `opacity`, and `metroBundleRendersThroughReact` asserts all three on the
  resulting Qt Quick items alongside the geometry.
- Border metrics come from React Native's own
  `BaseViewProps::resolveBorderMetrics` rather than a second implementation of
  the cascade here.
- **A mutation check caught a vacuous test before it was committed.** The first
  attempt at these assertions never reached the file: the script that patched
  both the app and the test asserted on stale app text and threw before writing
  either. The suite still passed, and changing the app's colour still passed —
  which is what exposed it. With the assertions actually present, changing
  `#204080` to `#207040` fails the case.
- Evidence level: host only, offscreen QPA. Nothing cross-compiled for QNX,
  nothing run on the Pi 4.

## A React bundle renders to Qt Quick items — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; `qt_react_host_host_test` is thirteen
  cases.
- `metroBundleRendersThroughReact` is the whole path: Metro bundles
  `apps/rn-playground` for `qnx`, Hermes evaluates it, `startSurface` runs
  `RN$AppRegistry.runApplication`, React reconciles, the commit reaches the
  delegate, and the tree lands as Qt Quick items. Asserted geometry: a 320x180
  outer item containing a 64x32 inner item — the dimensions the app's JSX
  declares, computed by Yoga.
- Mutation check: changing the app's width to 300 fails the case with
  `300 != 320`, so the number travels from the JSX through Metro, Hermes, React,
  Yoga and the mounting layer to the item. A weaker mutation — flipping
  `Platform.OS` to `ios` — changed nothing, which is worth knowing: `OS` is not
  load-bearing for this render.
- Four host responsibilities had to be added to get here, each a real gap rather
  than a stub: Hermes' microtask queue is off by default; `setTimeout` and
  friends are not engine builtins and are now backed by `QTimer`; React's work
  has to be drained explicitly with no running event loop; and
  `Platform.constants` is installed from the running system via `QSysInfo`.
- Two JavaScript platform overrides were required: `Platform.js` and
  `BaseViewConfig.js`. Both live in `@rn-qnx/react-native-qnx` and are selected
  by a Metro `resolveRequest` that swaps by resolved path.
- Evidence level: host only. The rendering surface is the offscreen QPA plugin,
  and nothing has been cross-compiled for QNX or run on the Pi 4.

## The bundle evaluates in Hermes — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default. `qt_react_host_host_test` is twelve
  cases.
- `metroBundleEvaluatesInHermes` loads the Metro-produced bundle into the Hermes
  runtime and it evaluates. `global.RN$AppRegistry` is installed afterwards,
  which is what `AppRegistryBinding::startSurface` requires.
- What unblocked it: `TurboModuleBinding::install` with a provider delegating to
  `DefaultTurboModules::getTurboModule`, plus 16 more React Native modules on the
  build, plus `QT_NO_KEYWORDS`.
- **Rendering still fails and no test claims otherwise.** Starting the surface
  aborts with `Cannot read property 'select' of undefined`: React Native's
  `Libraries/Utilities/Platform.js` re-exports `./Platform`, which resolves to
  nothing for an unknown platform, so there is no `Platform.qnx.js`. The error
  reporter then needs an ExceptionsManager TurboModule, which is why the abort
  message mentions it. Both are recorded in NEXT_ACTIONS.
- Finding: Qt's `emit` macro breaks `react/bridging/EventEmitter.h`, which
  declares a method of that name. Fixed with `QT_NO_KEYWORDS`.
- Evidence level: host only. Nothing has been cross-compiled for QNX or run on
  the Pi 4.

## Metro bundles for qnx — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default. `qt_react_host_host_test` is eleven
  cases; `validate:manifests` now reports two real package manifests.
- `@rn-qnx/metro-config-qnx` registers `qnx` in `resolver.platforms`; verified
  the resulting list is `["android","ios","qnx"]`.
- `apps/rn-playground` bundles for `--platform qnx` through Metro during the
  build, as a build artifact rather than a checked-in file. The bundle is 465 KB
  and contains `RN$AppRegistry`, `RN$stopSurface`, and the registered component
  name.
- `hostInstallsBridgelessGlobals` asserts the three globals React Native's
  JavaScript branches on, and that `RN$registerCallableModule` accepts a
  registration.
- **Evaluating the bundle does not work yet and no test claims it does.** It
  reaches `TurboModuleRegistry.getEnforcing('ExceptionsManager')` and throws;
  the bundle needs nine TurboModules. Asserting that would be asserting a
  failure, so the test stops at what Metro produced and the gap is recorded in
  NEXT_ACTIONS.
- Two problems fixed along the way, both real: pnpm keeps package files under
  `node_modules/.pnpm`, so Metro needed `watchFolders` and `nodeModulesPaths`
  pointed at the workspace root; and evaluating the bundle needed the bridgeless
  globals above before it got as far as TurboModules.
- Evidence level: host only. Nothing has been cross-compiled for QNX or run on
  the Pi 4.

## JavaScript drives the tree — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default. `qt_react_host_host_test` is now nine
  cases.
- `javaScriptBuildsTheTreeThroughFabricUIManager` runs JavaScript that calls
  `nativeFabricUIManager.createNode`, `createChildSet`, `appendChildToSet`, and
  `completeRoot` — the same binding surface React's renderer drives — and a Qt
  Quick item appears with Yoga's geometry computed from props JavaScript
  supplied.
- `componentPredicateAnswersForRegisteredNames` covers
  `bindHasComponentProvider`, including the `RCTView` to `View` normalisation
  and a negative case.
- Mutation check: expecting 161 instead of the 160 JavaScript passed failed the
  case with `160 != 161`, so the number really travels from JS through Yoga to
  the item.
- Evidence level: host only. This is not React and not a bundle — the JavaScript
  is hand-written against the binding. Nothing has been cross-compiled for QNX
  or run on the Pi 4.

## Runtime and renderer wired together — 2026-08-14

- CTest is 9/9 with `-DRNQNX_BUILD_HERMES=ON` and 7/7 by default. New:
  `qt_react_host_host_test`, seven internal cases.
- `QtReactHost` owns a Hermes runtime, a `RuntimeScheduler`, a
  `ContextContainer` carrying the scheduler under `RuntimeSchedulerKey`, and
  React Native's `Scheduler` built from a `SchedulerToolbox` filled with our
  `QtEventBeat`, `makeComponentRegistryFactory()`, and `QtSchedulerDelegate`.
- Cases: JavaScript evaluates and a value reads back through JSI; a thrown JS
  error is reported rather than swallowed; `global.nativeFabricUIManager` is
  installed, proving `UIManagerBinding` reached the runtime through our
  `RuntimeExecutor`; a surface commit mounts through the delegate with Yoga's
  geometry on the Qt item; a second commit updates the same item in place; and
  stopping a surface unmounts its items. A commit to an unregistered surface is
  reported, not silently dropped.
- Mutation check: making the delegate apply an empty mutation list instead of
  the transaction's failed three cases — `surfaceCommitMountsThroughTheDelegate`,
  `secondCommitUpdatesTheSameItem`, and `stoppingASurfaceUnmountsItsItems`.
  Reverted and re-run green. An earlier attempt at this mutation did not
  compile, so the first "pass" was a stale binary rather than evidence.
- Four React Native behaviours found while bringing this up, all recorded in
  the cited-seams ledger: the fallback descriptor must be
  `UnimplementedNativeView` (the other one segfaults on a null flavor), the
  descriptor registry is returned const and the fallback goes in via
  `const_cast` as React Native itself does, stopping any surface calls
  `RN$stopSurface` even when no React app ran, and `react_codegen_rncore` is
  codegen output that three renderer modules require.
- Evidence level: host only. JavaScript runs on the test thread, no bundle is
  loaded, no React renderer participates, and nothing has been cross-compiled
  for QNX or run on the Pi 4.

## Hermes builds and runs on the host — 2026-08-14

- Hermes `hermes-v250829098.0.16` (commit `90f2385`) builds from source on
  macOS ARM64: 418 targets, no errors, producing `libhermesvm.dylib`.
- `rnqnx_hermes_probe` evaluates JavaScript through JSI and asserts four things:
  arithmetic, an ES6 shape (class + spread + destructuring + reduce), an
  **async arrow function body actually executing**, and a host function called
  from JS returning a value to C++.
- The async-arrow case is the react-native-linux landmine, where a Hermes build
  silently did not run `async () => {}` bodies. It passes here and is now a
  permanent regression test.
- Mutation check: emptying the async arrow's body produced
  `RNQNX_HERMES_FAIL async-arrow: got 0, expected 7`. Reverted and re-run green.
- Two configurations both pass: CTest 7/7 with Hermes off (the default), 8/8
  with `-DRNQNX_BUILD_HERMES=ON`. Hermes is opt-in because it is a long build
  that most changes do not need.
- One integration problem found and fixed: React Native ships two CMakeLists for
  JSI. The outer globs `jsi/*.cpp` and includes `JSIDynamic.cpp`; the inner
  builds `jsi.cpp` alone. Hermes adds the inner one, so `AppRegistryBinding`
  failed to link on `valueFromDynamic`. That translation unit is added back
  explicitly rather than by duplicating a JSI copy.
- Evidence level: host only. Hermes has not been cross-compiled with `qcc`, has
  not run on the Pi 4, and no React Native bundle has been loaded — the probe
  evaluates source strings directly.

## Shadow tree to Qt items, end to end — 2026-08-14

- CTest is 7/7. New: `shadow_tree_mounting_host_test`, five cases.
- The pipeline under test is entirely React Native's: a real `ShadowTree` is
  committed from C++, React Native's differ computes the mutations,
  `MountingCoordinator::pullTransaction` hands over a `MountingTransaction`, and
  the adapter applies it to Qt Quick items. The geometry asserted is Yoga's,
  computed by React Native, not by this repository.
- Cases: a fixed 120x60 view reaches the item; two flex children split a 160pt
  column 1:3 with correct stacking offsets; 16pt padding offsets a child's
  origin; a second commit resizes the same item in place rather than replacing
  it; removing a child from the tree unmounts its item.
- Finding: view flattening. The first run produced exactly one mutation, an
  Update of the root sized to the child, and no Create — React Native flattens
  views carrying only layout props. The test sets `collapsable = false`, and the
  behaviour is documented in the cited-seams ledger rather than worked around.
- Mutation check: removing `setY` from the mounting layer failed
  `flexChildrenSplitTheRootWidth` (`0 != 40`) and `paddingOffsetsChildOrigin`
  (`0 != 16`). Reverted and re-run green.
- Evidence level: host only. No JavaScript runs — the tree is committed from
  C++ — and nothing has been cross-compiled for QNX or run on the Pi 4.

## React Native renderer builds from source — 2026-08-14

- 41 React Native 0.86.2 modules now build on the macOS host by adding React
  Native's own per-module `CMakeLists.txt` files: the renderer core, graphics,
  mounting, componentregistry, view/root/scrollview/text components,
  attributedstring, textlayoutmanager, uimanager, runtimescheduler, css,
  consistency, observers, performance timeline, jsinspector, cxxreact, Yoga,
  JSI, and their support modules.
- Third-party is now compiled rather than header-only: `folly_runtime` builds
  all 29 sources React Native lists, plus real glog, fmt, and double-conversion
  libraries and React Native's trimmed boost headers.
- `rnqnx_fabric_adapter` links `react_renderer_mounting` instead of compiling
  `ShadowViewMutation.cpp` directly.
- CTest remains 6/6 with every existing case passing against the real modules.
- Five problems found and fixed during bring-up, each by using React Native's
  own mechanism rather than working around it: the platform selector had to be
  included for the generic `cxx` paths; `REACT_ANDROID_DIR` had to be set so
  `target_merge_so` resolves (it is a no-op off Android); nine `platform/cxx`
  include directories had to be added globally, matching React Native's
  assembler; Yoga's target is `yogacore` and needed the `yoga` alias; and OBJECT
  library objects had to be attached explicitly because they are not archived
  into a static library that links them.
- Evidence level: host-compiled and host-linked. None of these modules has been
  cross-compiled with `qcc` or run on the Pi 4, and no bundle has been executed.

## React Native headers and mutation adapter — 2026-08-14

- CTest is 6/6. New: `rnqnx_rn_headers_probe` and
  `shadow_view_mutation_adapter_host_test` (seven internal cases).
- React Native 0.86.2 renderer headers compile here for the first time, against
  fetched folly 2024.11.18.00, fmt 12.1.0, glog 0.3.5, double-conversion 1.1.6,
  and fast_float 8.0.0. Boost is not needed and is not fetched.
- `rnqnx_rn_headers_probe` enforces every mirror in the cited-seams ledger with
  `static_assert`, including the five `ShadowViewMutation::Type` values and the
  identity of `Tag` and the layout `Float`.
- The adapter applies real `ShadowViewMutation` values built through React
  Native's own static factories: create/insert with initial layout, index
  ordering, update, remove-then-delete, nested trees, and reported (not skipped)
  failures for unmapped components and missing parents.
- Two problems found and fixed during bring-up: glog defines a `CHECK` macro
  that collided with the test harness, renamed to `EXPECT`; and
  `RN_DEBUG_STRING_CONVERTIBLE` pulled `ShadowView.cpp`'s debug-string overloads
  into the link, resolved with React Native's own `REACT_NATIVE_PRODUCTION`
  switch rather than by stubbing the symbols.
- Mutation check: forcing the adapter's Insert to always use index 0 made
  `insertOrderFollowsMutationIndex` fail at the expected assertion. Reverted and
  re-run green.
- Evidence level: host only. Transactions are constructed in tests, not pulled
  from a live surface, and nothing has been cross-compiled for QNX.

## Fabric mounting layer host tests — 2026-08-14

- CTest is 4/4: `rnqnx_g0_cxx20_probe`, `qnx_screen_surface_host_test`,
  `qtquick_mounting_layer_host_test`, `rnqnx_qtquick_host_probe`.
- `qtquick_mounting_layer_host_test` reports fifteen internal cases, all passing
  under strict warnings, ASan, and non-recovering UBSan on the offscreen QPA
  plugin.
- Coverage: root borrowed and undeletable; create leaves items unparented;
  insert ordering including insert-at-0 ahead of existing children; unknown
  parent, unknown tag, and out-of-range index rejected; remove detaches without
  destroying; remove rejects index disagreement and wrong-parent claims; destroy
  releases both tag and item; destroy detaches a still-mounted item; a nested
  three-level tree is torn down with the layer; layout applies absolute
  geometry; unregistered type, broken QML source, non-item type, and duplicate
  tag each fail with the specific error; re-registering a type invalidates the
  cached component.
- Mutation check: forcing the restacking branch off made
  `insertParentsAndOrdersChildren` fail at the expected assertion, so the
  ordering coverage is not vacuous. The change was reverted and the suite
  re-run green.
- Evidence level: host only. No React Native header is compiled here, no QNX
  translation unit exists, and nothing has run on the Pi 4.

## Qt Quick host bring-up — 2026-08-14

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.21000099,
  Homebrew Qt 6.11.1 at `/opt/homebrew/opt/qt`.
- `find_package(Qt6 6.8 COMPONENTS Core Gui Qml Quick QUIET)` resolves with no
  `CMAKE_PREFIX_PATH` hint on this machine; CMake logged
  `Qt 6.11.1 found; building Qt Quick host targets`.
- `rnqnx_views_qt` and `rnqnx_qtquick_host_probe` compile clean under
  `-Wall -Wextra -Wpedantic -Werror` against public Qt headers only.
- CTest is 3/3: `rnqnx_g0_cxx20_probe`, `qnx_screen_surface_host_test`, and
  `rnqnx_qtquick_host_probe` (offscreen QPA).
- Probe output:

  ```text
  RNQNX_QTQUICK_HOST_OK items=1000 create_ms=12.856 update10_ms=0.456 destroy_ms=0.516 platform=offscreen
  ```

- Evidence level: host-compiled, host-executed. This is not Qt-on-QNX evidence.
  No Qt code has been cross-compiled or run on the Pi 4, the QNX QPA plugin over
  Screen is unbuilt, and G0-B and G0-C remain open. Nothing here promotes a QNX
  validation row.

## Latest host validation — 2026-08-14

Re-ran the full host suite before committing the batch.

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.21000099.
- `pnpm format:check` failed first on `docs/cited-seams.md` and
  `docs/getting-started/compatibility-matrix.md`; `pnpm format` fixed both and
  the re-run passed.
- `pnpm validate:macos` then completed all eight stages with exit 0.
- CTest is now 2/2, not 1/1: `rnqnx_g0_cxx20_probe` (0.27 s) and
  `qnx_screen_surface_host_test` (0.38 s, eight internal cases). The probe
  passing here is host evidence only — G0-A requires that binary to run on the
  Pi 4, which has not happened.
- `pnpm install --frozen-lockfile` reported "Already up to date".
- Not-applicable stages are unchanged: no `tests/js` files, no package
  manifests, no `nitro.json`.

The dated table below remains the detailed per-check record; only the CTest
count and the two Prettier files changed.

## Host validation — 2026-08-13

- Host: macOS 26.6.1, Darwin 25.6.0 arm64, Apple Clang 21.0.0.
- Repository package manager: pnpm 11.21.0.

| Check                                 | Result         | Evidence                                                                                       |
| ------------------------------------- | -------------- | ---------------------------------------------------------------------------------------------- |
| `pnpm validate:macos`                 | Passed         | All eight host stages completed with exit 0.                                                   |
| Prettier                              | Passed         | All supported files match Prettier 3.9.6.                                                      |
| TypeScript                            | Passed         | TypeScript 6.0.3 `tsc --noEmit`, strict JS checking enabled.                                   |
| Installed dependency/source contracts | Passed         | Exact pins, Expo graph, Hermes tag, and 19 named installed-source checks passed.               |
| Scoped `xcode>uuid` compatibility     | Passed         | uuid 11.1.1 resolves; xcode's call site and CommonJS `uuid.v4` export are present.             |
| `pnpm audit --audit-level moderate`   | Failed         | Two high `image-size` DoS advisories remain; no fixed registry release exists.                 |
| JavaScript tests                      | Not applicable | `tests/js` contains no test files; the runner reported an explicit skip.                       |
| QNX Screen C++ host target            | Passed         | Strict C++20, ASan, and non-recovering UBSan; CTest 1/1 passed.                                |
| Internal Screen wrapper cases         | Passed         | Eight cases cover setup, ARGB fill, buffer refresh, cleanup, moves, and init/runtime failures. |
| Workspace package build               | Not applicable | No app/package manifests exist yet; 15 directories are intentionally scaffold-only.            |
| Manifest validation                   | Passed         | 0 manifests and 15 scaffold-only directories reported accurately.                              |
| Architecture validation               | Passed         | Required tracked scaffold directories and production files are present.                        |
| Nitrogen generated-output check       | Not applicable | No `nitro.json` exists; QNX generation has not been designed.                                  |
| `git diff --check`                    | Passed         | No whitespace errors.                                                                          |

The CTest count is one executable; that executable reports eight independent
test cases. Host tests compile against `tests/cpp/qnx-screen-fake`, never an
installed QNX header or `libscreen`.

## Expected unsupported-host diagnostic — 2026-08-13

`bash scripts/configure-qnx.sh aarch64 Debug` exited 1 on macOS as expected. The
primary CMake error was:

```text
Environment variable QNX_HOST is not set.
```

CMake also emitted downstream compiler/generator errors after the toolchain
fatal error. This is not a QNX build attempt and is not a pass.

`bash scripts/doctor.sh` also exited 1 as expected: eight QNX environment errors
and five host/target warnings. Its CMake, Ninja, Node, exact pnpm, Python, and
installed dependency/source checks all passed.

## QNX validation status

| Suite                                      | Status  | Reason                                                                     |
| ------------------------------------------ | ------- | -------------------------------------------------------------------------- |
| Installed QNX `screen/screen.h` audit      | Not run | QNX SDP 8.0.2 is not installed in this workspace.                          |
| QNX AArch64/x86-64 cross-compilation       | Not run | No `QNX_HOST`, `QNX_TARGET`, `qcc`, `q++`, or licensed sysroot.            |
| `libscreen` linkage                        | Not run | QNX Screen development library is unavailable.                             |
| Virtual/physical QNX target runtime        | Not run | No QNX target is configured.                                               |
| Target resource, presentation, and cleanup | Not run | Requires the installed Screen service, BSP/driver, compositor, and target. |

Do not promote any QNX queue state from host evidence. The exact continuation is
recorded in `QNX_VALIDATION_QUEUE.md`.

## Dependency-audit exception

The original audit also reported vulnerable uuid 7.0.3 through xcode. A scoped
override to uuid 11.1.1 removed that moderate advisory, and its installed call
shape is part of `validate:dependencies`. The two remaining high findings are
`UPSTREAM-DEP-001`; they are recorded rather than ignored. Consequently, “full
host validation passed” does not mean “registry security audit is clean.”
