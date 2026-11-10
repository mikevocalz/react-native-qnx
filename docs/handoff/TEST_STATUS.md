# TEST_STATUS.md

## 2026-10-03 braces security backport

Host: Linux cloud workspace, Node 26.10.0, pnpm 12.8.1. No QNX SDK or target.
The frozen install applies the pinned five-library-file backport. Local
TypeScript, JavaScript security/compatibility/tamper/audit-policy tests,
manifest/dependency contracts, peers, package build, architecture, codegen
applicability, and production QNX Metro bundling pass (exit 0). Prettier passes.
The guarded audit passes with an explicit locally-mitigated GHSA message;
raw registry audit still reports braces 3.0.3. Linux C++ host validation passes
2/2 CTest executables with GCC 14.2, CMake 3.31.10 and ASan/UBSan (exit 0).
CMake was initially absent and was installed before this successful run. Optional
Qt targets were skipped because Qt 6.11.1 is unavailable. Remote CI is pending.

Independent Node 24.19.0 review: 774 upstream/backport tests and 48,277
additional differential/boundary assertions pass. Original stack exhaustion
reproduced before patch; patched code rejects excessive depth. These are host
results only. See `docs/security/braces.md` for scope and removal instructions.

## 2026-10-04 bundled React probe (Linux x86-64 cloud host)

- Node 26.10.0, pnpm 12.8.1, CMake 3.31.10, Clang 19.1.7 and Qt 6.11.1.
- `pnpm validate:qnx-react`, `pnpm typecheck`, `pnpm test:js`,
  `pnpm validate:architecture`, `pnpm codegen:check`, `pnpm build:packages`,
  `pnpm format:check`: exit 0. Source checks alone are not runtime evidence.
- Production Metro `react-probe.js --platform qnx --dev false`: exit 0,
  172 real React/React Native modules. `pnpm test:cpp:host`: initial basic
  smoke 2/2 passed with optional Qt skipped; not evidence for this executable.
- Native integration was tested with companion PR #28 commit
  `24ba958d046066f54faae30847de1c52b972f4ba`. This probe revision's
  source/fixture/CMake files were overlaid temporarily, then removed and the
  companion checkout restored. This is a paired integration result; main alone
  lacks the required RN 0.88 build/scheduler/error-handler fixes.
- `cmake --build build/linux-host-renderer --target rnqnx_react_render_host_probe`:
  exit 0. `ctest --test-dir build/linux-host-renderer -V -R
'^rnqnx_react_render_host_probe'`: exit 0, **2/2** registered tests passed.
  Positive output: `RNQNX_REACT_RENDER_OK platform=host bundled-react
initial=160x100/48x24 update=72x24 unmount=empty geometry-only transactions=3`.
  Missing, empty, throwing and non-registering bundles all reject cleanly.
- `cmake --build --preset linux-host-renderer` and
  `ctest --preset linux-host-renderer --output-on-failure`: exit 0, **11/11**
  combined CTests passed, including all **45 QtReactHost** cases (59.41 seconds).
- Configuration used the companion Linux renderer preset, Qt/host-sysroot prefix,
  and `-DCMAKE_CXX_FLAGS=-I/tmp/qnx-host-sysroot/usr/include
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`. A stale compiler-detection cache was
  recovered by restoring `CMAKE_EXECUTABLE_FORMAT=ELF` after inspecting the actual
  ELF compiler output; dependencies were rebuilt afterward, not bypassed.
- The positive-runner harness also rejects nonzero exit with a success marker,
  stderr-only success, missing success and an explicit error marker. Those
  harness-only checks do not constitute React execution evidence.
- The existing main-line braces dependency audit is handled separately in #27.
  QNX SDP 8.0.5 compilation, Pi execution and pixel/on-glass evidence: **not run**.

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

## Current QNX integration source/tooling validation — 2026-10-02

- Main includes merged PRs #7–#12 on top of the SDK-58 baseline:
  current SDP 8.0.5 cross-build evidence tooling, the RN 0.88 Hermes-QNX source
  rebase, physical Pi evidence verification, the synchronous Nitro QNX runtime,
  Expo Modules Core QNX JSI bootstrap, and Expo QNX autolinking.
- PR #8 head passed `Hermes QNX source check`, `Pi tooling quality`, and
  `SDK 58 validation`.
- PR #9 head passed `Nitro QNX source check`, `Pi tooling quality`, and
  `SDK 58 validation`.
- PR #10 head passed `Nitro QNX source check`, `Pi tooling quality`, and
  `SDK 58 validation`.
- PR #11 head passed `Expo Modules QNX source check`,
  `Nitro QNX source check`, `Pi tooling quality`, and `SDK 58 validation`.
- PR #12 head passed `Expo QNX autolinking`,
  `Expo Modules QNX source check`, `Nitro QNX source check`, and
  `Pi tooling quality`. Its SDK-58 validation run was cancelled during the
  merge sequence; the immediately prior merged #11 head had the full SDK-58
  suite green.
- The hardened hardware lane now defaults to three Screen launch/exit cycles,
  requires all three to initialize and shut down cleanly, and automatically
  validates/hashes the evidence into `manifest.json`.
- **Evidence boundary:** none of the above replaces the still-missing physical
  QSTI run. Hermes/Nitro/Expo source implementation is not being promoted to
  QNX runtime proof until it is cross-built with the licensed current SDP and
  executed on the target.

## Current Expo SDK 58 / main validation — 2026-10-01

- Merged code baseline: `47c3d668b0ba61eaf81ce8083b87c3bf82c876ce`.
- Main workflow `SDK 58 validation` run 36896516617: **passed**.
- Main workflow `Pi tooling quality` run 36896516638: **passed**.
- Exact direct line: Expo 58.0.0, React Native 0.88.0-rc.3, React 19.3.0,
  Metro 0.87.1, Nitro/Nitrogen 0.37.1, TypeScript 7.0.2, Node 26+, pnpm
  12.8.1.
- Frozen `pnpm-lock.yaml` install passes.
- Prettier passes across the merged repository.
- `validate:dependencies`, TypeScript, and `pnpm peers check` pass.
- Metro 0.87.1 produces a production bundle for `--platform qnx`; the
  validation run reports a roughly 772 KB bundle.
- Repository architecture/package/codegen-state checks pass.
- Linux C++ host smoke tests pass with CMake 3.31.10.
- `pnpm audit:dependencies` passes at the configured moderate threshold after
  scoped patched transitive overrides.
- Pi scripts pass Bash syntax and ShellCheck.
- **Evidence boundary:** this current CI does not install the optional full Qt +
  Hermes host renderer stack and does not use a licensed QNX toolchain.
  Therefore it does not replace the historical full host-renderer evidence or
  establish current QNX runtime proof.
- **Next target evidence:** QNX SDP 8.0.5 + QSTI 0.5.0 build 50 on physical
  Raspberry Pi 4/5 via `pnpm pi:preflight` and `pnpm pi:install`.

The dated sections below are historical evidence from the pre-SDK58 line unless
they explicitly say otherwise.

## QNX cross-compile matrix on Windows host — 2026-08-16

- QNX SDP 8.0.2 is installed at `C:\Users\Owner\qnx800`; `qcc -V`/`q++ -V` confirm
  both the `gcc_ntoaarch64le` and `gcc_ntox86_64` targets.
- `vnext/cmake/qnx-aarch64le.toolchain.cmake` is fixed to find the Windows-hosted
  QNX compilers/archiver (`qcc.exe`, `q++.exe`, `aarch64-unknown-nto-qnx8.0.0-
ar.exe`) and pass the `-Vgcc_ntoaarch64le` target flag.
- `cmake/toolchains/qnx-8.0.cmake` is fixed to find the Windows-hosted QNX
  compilers, select the right target-specific archiver/ranlib for aarch64 and
  x86_64, and pass the matching `-V<variant>` flag.
- All four QNX presets configure and build on this Windows PC:
  - `qnx-aarch64-debug` → `rnqnx_playground` (AArch64 ELF, links `libscreen.so.1`),
    `libqnx_platform.a`, `rnqnx_g0_cxx20_probe`
  - `qnx-aarch64-release` → same targets, Release
  - `qnx-x86_64-debug` → `rnqnx_playground` (x86_64 ELF), `libqnx_platform.a`,
    `rnqnx_g0_cxx20_probe`
  - `qnx-x86_64-release` → same targets, Release
- `cmake --fresh --preset qnx-pi4` and `cmake --build --preset qnx-pi4` produce a
  QNX AArch64 `rnqnx_g0_cxx20_probe`. This SDP's libc++ disables experimental
  `stop_token`, so the probe falls back to `std::thread` and reports
  `jthread=unsupported`; ranges and designated initializers still pass.
- Evidence level: cross-compiled QNX ELF on a Windows PC. Nothing has run on a
  QNX target yet.

## TextInput types, and an event-beat use-after-free — 2026-08-15

- CTest 9/9; twenty `qt_react_host_host_test` cases, 55 host cases in total.
- The native `TextInput` seam works. React Native's textinput descriptor under
  `platform/ios` is portable C++ — `TextInputShadowNode` and `TextInputProps`
  include only `react/`, Yoga and the standard library, while the Android one
  reaches for fbjni and the Java `FabricUIManager` to read theme padding
  (`AndroidTextInputComponentDescriptor.h:41-47`) — so that is the one this
  platform registers. Its component name is `"TextInput"`
  (`TextInputShadowNode.cpp:12`), which React Native's own view names map onto
  (`componentNameByReactViewName.cpp:62-64`).
- `textInputMounts` renders the component, asserts `text`, font size and
  `readOnly` reached the Qt item, sends a real key event to the focused item,
  and then asserts both that the item now reads `typedX` and that JavaScript
  received `onChange` carrying that same string. Mutation check: never applying
  `text`, and putting the wrong string in the event metrics, each fail. A third
  mutant — deleting the signal connection — did not compile, so it was re-run as
  a connection to `editingFinished()`, which does compile and does fail.
- React Native's `TextInput.js` branches on `Platform.OS` with no third branch,
  so it renders nothing on `qnx`. The test therefore reaches the native
  component directly. The JavaScript wrapper is a separate, still-open problem.
- **A real use-after-free was found by the new tests, not by review.** The event
  beat asked the Qt loop for a tick by posting a queued call to
  `QCoreApplication::instance()`, which outlives every host. A host torn down
  with a beat in flight therefore left a pending `induce()` on a destroyed beat,
  and the next thing to pump the loop dereferenced it. The receiver is now the
  beat's own `QObject`, which `~QObject` drains. React Native's `OwnerBox` does
  not help here: it is checked inside `induce()` (`EventBeat.cpp:62-66`), which
  is already too late for a dangling `this`.
- The bug was intermittent — 3 failures in 40 runs — because a use-after-free
  need not touch an unmapped page. `qt_react_host_host_test` now builds with
  `-fsanitize=address`; ASan's allocator is installed process-wide by
  interposition, so freed memory is poisoned even though React Native and Qt are
  not themselves instrumented. With the bug reintroduced the suite fails 5 runs
  out of 5; with it fixed, 30 runs out of 30 pass.
- A dedicated regression case for the beat was written and then **deleted for
  being vacuous**: it passed with the bug present, because `~Fixture` calls
  `stopSurface`, which drains the posted beat while the host is still alive. The
  invariant is covered by the suite under ASan instead, which is measured above
  rather than assumed.
- Evidence level: host only, offscreen QPA. Nothing has been cross-compiled for
  QNX or run on the Pi 4.

## FlatList virtualises, and scroll events reach JavaScript — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; nineteen `qt_react_host_host_test`
  cases, 54 host cases in total.
- `scrollEventsReachJavaScript` drives the mounted `Flickable`'s `contentY` to
  120 and asserts in the runtime that `onScroll` ran, that
  `contentOffset.y` is 120, and that `contentSize.height` is the 300 this
  platform computed from the Flickable's children. Mutation check: removing
  either signal connection leaves `globalThis.__scrolls` unset; adding 1 to the
  offset or doubling the content height fails the corresponding assertion. All
  three mutants compiled before being run, which is what makes the result mean
  anything.
- `flatListRendersAndVirtualises` renders a 20-row `FlatList` with
  `initialNumToRender: 4` and `windowSize: 1` in a 100pt viewport, then scrolls
  to offset 300 in 30pt steps. The rendered cells settle at offsets
  `{0, 30, 60, 90}` and `{270 … 390}`: the retained initial region plus a live
  window around the scroll, with rows 4 through 7 released. The test asserts
  both halves — that the window followed the scroll, and that the cells it left
  are gone.
- Nothing native was missing for `FlatList`. `onLayout` already reached
  JavaScript with correct geometry, and `onContentSizeChange` followed from the
  content container's own layout event; the list read a 100pt viewport and a
  420pt content length. What was missing was `onScroll`, and a first attempt
  that scrolled instantly and pumped a fixed number of iterations produced a
  misleading result twice over: `VirtualizedList` batches its window update
  behind a real 50ms `setTimeout`, and this platform's timers are `QTimer`s, so
  the pump has to cover wall-clock time.
- Two assertions in the first draft of the `FlatList` case were wrong about the
  platform rather than about the platform being wrong. Expecting fewer than 20
  cells under the default `windowSize` of 21 was meaningless — 21 viewports
  cover 2100pt against 600pt of content — and expecting the leading cell to be
  released ignored that `initialNumToRender` is deliberately retained. Both were
  corrected against measured offsets, not assumed ones.
- Qt notifies `contentX`/`contentY` by signal, but `Flickable`'s class is
  private to Qt Quick and these items are known only through their metaobject,
  so the functor-based `connect` is unavailable and the string-based one needs a
  real slot. `QtScrollBridge` is that slot, parented to the item so the
  connection dies with it. `QT_NO_KEYWORDS` is set on this target, so it
  declares `Q_SLOTS` rather than `slots`, and its header is listed in the
  target's sources because `AUTOMOC` does not scan headers found only through
  include paths.
- Evidence level: host only, offscreen QPA. Nothing has been cross-compiled for
  QNX or run on the Pi 4.

## ScrollView scrolls — 2026-08-14

- CTest 9/9 with Hermes, 7/7 by default; seventeen host cases.
- The playground renders a 100x50 `<ScrollView>` containing a 100x300 view. The
  test asserts the child is mounted into the Flickable's `contentItem` rather
  than onto the Flickable itself, and that `contentHeight` is 300 against a 50pt
  frame. The rendered frame contains exactly 5000 purple pixels — 100x50 — which
  is the content clipped to the viewport.
- Two platform behaviours had to be handled:
  - A Flickable scrolls its `contentItem`, so children mounted directly on it
    would never move. Any item exposing a `contentItem` now receives its children
    there; a plain Item has no such property and is unaffected.
  - Qt does not derive `contentWidth`/`contentHeight` from children the way
    React Native does, so they are recomputed on every insert, remove, and child
    layout change.
- `DeviceInfo` was required first: `Dimensions` reads it and `ScrollView` reads
  `Dimensions`. It reports Qt's primary screen geometry and device pixel ratio.
  `Dimensions.js:126` reads `getConstants().Dimensions`, so the metrics are
  nested under that key — a flat object throws "Cannot read property 'screen' of
  undefined".
- A layout surprise worth recording: the scroll view first laid out at zero
  height. React Native's `ScrollView` base style is shrinkable, and the parent's
  children already overflowed it, so Yoga collapsed it. The app sets
  `flexShrink: 0`; the platform was behaving correctly.
- Mutation check, build verified first: removing the `contentItem` redirect
  fails the case.
- Evidence level: host only. Nothing cross-compiled for QNX, nothing run on the
  Pi 4.

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
- `find_package(Qt6 6.11.1 COMPONENTS Core Gui Qml Quick QUIET)` resolves with no
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

## PR #111 — Qt v2 validation automation (2026-11-10)

Candidate: `295f7240099c7371dc8988f4d411b1840888a415`; Linux x86-64,
Node 26.13.0, pnpm 12.8.1. No native source changed in this follow-up.

| Command or lane                                                               | Outcome         | Scope                                                                                                      |
| ----------------------------------------------------------------------------- | --------------- | ---------------------------------------------------------------------------------------------------------- |
| `pnpm install --offline --frozen-lockfile --store-dir /workspace/.pnpm-store` | Exit 0          | Exact installed dependency graph; no dependency change                                                     |
| `node tests/js/host-renderer-contract.test.mjs`                               | Exit 0          | Validator behavior with fake executables, including each missing v2 test and disabled-feature registration |
| Same new regression against the original validator                            | Expected exit 1 | Reproduces acceptance of a missing enabled-feature test; not native proof                                  |
| `pnpm typecheck`                                                              | Exit 0          | Repository JS/TS contract                                                                                  |
| `pnpm qt:svg:check`                                                           | Exit 0          | RN 0.88.1 generation matches all 11 committed files                                                        |
| Changed-file Prettier, `pnpm validate:docs`, `git diff --check`               | Exit 0          | Formatting, handoff/schema and whitespace checks                                                           |
| New hosted full-renderer ON/OFF matrix                                        | Pending         | Actual CTest inventory/JUnit and effective CMake options are uploaded by the workflow                      |
| QNX cross-build, VM/device, Marathon compositor                               | Not run         | Existing target acceptance gates remain open                                                               |

The native evidence in [v2 validation](../v2/validation.md) predates the CI
coverage correction. Passing mocked orchestration cannot substitute for the
new enabled hosted lane or target execution.
