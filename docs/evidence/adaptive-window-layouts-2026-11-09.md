# Adaptive window layouts — host validation, November 9, 2026

## Candidate and scope

This is the implementation of the owner-approved adaptive-layout proposal for
the React Native QNX component gallery and generated starter. The PR is stacked
on gallery/desktop PR #104 at `3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94`.
Its recorded physical QNX history belongs to that earlier candidate and is not
reused as target qualification for these changes.

The new implementation uses the existing React Native 0.88.1, Fabric, Yoga,
Hermes, JSI and Qt Quick path. The dependency lock and exact package versions
remain unchanged. No premium Forge package, new renderer, WebView or Android
runtime is introduced. `DeviceInfo` is React Native's internal compatibility
module; the additional public JavaScript layout helpers do not create a new
application-facing native service or bypass the pinned Nitro v1 API policy.

The validated local runtime, UI and test source is commit
`87793906dae1653f7ceca57097b51192f431c7aa`, tree
`a0a01f4b406cdd023d3f6f78f7bb7eb3e8b128ad`. It is published as
[`56093f7`](https://github.com/mikevocalz/react-native-qnx/commit/56093f7eaab414f206cd8f679abb36b0c2caea6c)
with **the identical Git tree**. The connected GitHub integration supplied the
published commit metadata because local Git had no write credentials; no source
bytes changed. The following commit adds evidence only.
The exact log and artifact hashes are recorded in the adjacent machine-readable
receipt. This document distinguishes host evidence,
source review and missing target evidence.

## User-visible changes

The real gallery and both starter-generation paths share
[`@rn-qnx/react-native-qnx/adaptive`](../../packages/react-native-qnx/adaptive/README.md).
The shell uses compact navigation at narrow widths and a sidebar from 840
logical units. Its short-height layout leaves room for content and makes all
six destinations reachable through the scrolling navigation dialog. The grid
chooses one, two or three columns from the measured main-content width, with a
300-unit preferred card width and 16-unit gutters; it never calculates columns
from the physical monitor width. Extra-wide windows keep at most three readable
columns. Forms and dialogs have bounded widths, and cards use natural heights,
readable text and small corner radii.

Visited pages stay mounted in stable, absolute-fill layers with unchanged
geometry. Inactive layers are transparent and excluded from input. The change
uses neither `display: none` nor route-dependent parent ancestry, because the
former removes Fabric native subtrees and the latter can remount editors and
lists. Zustand retains the application values; stable native items retain the
caret and list scroll position. This is bounded by the six gallery destinations,
with existing FlatList/SectionList virtualization retained.

Qt text measurement and painting now share font resolution, font scale,
multiplier caps, weight, italic style, letter spacing and paragraph line height.
The default sans face is embedded Roboto, independent of the fonts on a minimal
QNX image. TextInput renders its native fill, border and corner radius, while its
editor and placeholder use Yoga's already-resolved content insets. Removing a
field background restores transparency. A native focus outline and
View/Pressable focus events support keyboard use; the outline is a disabled
paint-only decoration so it cannot intercept React pointer hit testing.

## Native metrics and ownership

`QtWindowMetricsObserver` observes the actual QWindow and usable React client
item. Production startup waits for positive client geometry before evaluating
the bundle, installs the observer, and starts one Fabric surface. Window and
screen sizes are distinct logical values; window DPR comes from the actual
window, screen DPR from its screen, and font scale remains independent. Baseline
font scale is 1.0. The validated host `setFontScale()` seam is not an implemented
system accessibility-preference provider.

Coalesced geometry, screen and DPR changes update that same Fabric surface's
constraints and LayoutContext, then deliver the standard `didUpdateDimensions`
event through the existing React Native invoker. Cached current observations
serve initial `Dimensions` consumers. A second observer binding is rejected,
and manual `resizeSurface()` on an observed surface is rejected rather than
allowing Yoga and Dimensions to disagree. The observer and pending module
notifications become inert before runtime disposal.

The existing Qt native modal overlay now contains keyboard focus in the topmost
modal, traverses enabled tab-eligible descendants, blocks background keyboard
input, handles Escape through React's `onRequestClose`, and restores a valid
previous target. Nested overlays and detached/disabled button behavior are
covered separately from the gallery. These checks do not establish complete
screen-reader, IME or native window-manager behavior.

## Source inspection and applied skills

| Skill or source                                           | Exact revision/version                                                 | Decision applied                                                                                                                                                                          |
| --------------------------------------------------------- | ---------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Repository `qnx-native-api` and `qnx-platform-validation` | Repository files at prerequisite `3b414f1`; read before implementation | Standard RN compatibility remains distinct from new Nitro service APIs; native tests are required in both flag configurations; host and target evidence stay separate.                    |
| Margelo `api-design` → `build-nitro-modules` → `cpp`      | `1e9e17be6f41c838db472d6a4b943fc26355a3be`, read in that order         | Reuse existing owner-thread runtime/invoker, focused metric types, explicit invalid-input behavior and owned callback cleanup. No new generated native API was required.                  |
| Official Qt UI design                                     | Version 1.1, source `062f7c9f0e16b7d2f66cd55e449a3ed85fba699e`         | Prefer content-driven sizing, consistent typography and native input behavior; Yoga remains the layout authority.                                                                         |
| Official Qt C++ review                                    | Version 3.0, same pinned Qt skills source                              | Deterministic changed-line lint followed by A1–A6 source review and a consolidated report. This caught a false-positive DPR test and reviewed modal/observer teardown.                    |
| Pinned RN 0.88.1 source                                   | Installed, locked source                                               | Inspected Dimensions, useWindowDimensions, PixelRatio, DeviceInfo/NativeEventEmitter, text props, LayoutMetrics content insets and Fabric surface/layout contracts before implementation. |
| Qt 6.11.1 source/interfaces                               | Installed public headers and QML types                                 | Verified window/screen/DPR signals, text font/line-height behavior, native focus and Rectangle corner properties.                                                                         |

The Qt review is automated engineering review, not a vendor endorsement or a
human approval. Human platform DRI and independent QNX/Qt reviewer remain
unassigned; the repository owner must assign them before merge. Implementation
was split across native metrics/focus, typography and adaptive application/
packaging workstreams; integration and cross-file review were performed
separately. The [Qt review report](adaptive-window-layouts-qt-review-2026-11-09.md) records
findings and their dispositions; the [UI receipt](adaptive-window-layouts-ui-validation-2026-11-09.md)
records JavaScript, type and packaging checks.

## Validation environment and commands

Linux x86_64; Qt 6.11.1 offscreen with the software Qt Quick backend; Clang
18.1.3; CMake 3.31.10; Ninja; Node **26.0.0** and pnpm 12.8.1. This is not a
Wayland session, a Marathon shell, a QNX VM or a physical QNX device. The local
QNX SDK/toolchain is unavailable (`qcc`, `QNX_HOST` and `QNX_TARGET` were not
configured), so no cross-build or deployment was attempted.

The full renderer is required, not silently skipped:
`RNQNX_REQUIRE_FULL_HOST_RENDERER=ON`, `RNQNX_FETCH_REACT_NATIVE_DEPS=ON`,
`RNQNX_BUILD_HERMES=ON`, `BUILD_TESTING=ON`, Debug and host sanitizers ON.
The receipt records separate OFF and ON values for desktop, input lifecycle,
TextInput contract and accessibility. No new feature flag defaults to ON.

The retained commands include:

```sh
pnpm typecheck
pnpm validate:dependencies
pnpm validate:architecture
pnpm validate:manifests
pnpm validate:docs
pnpm test:js
# The machine-readable receipt contains the exact OFF/ON target lists.
cmake --build build/linux-adaptive --target <registered-test-targets> --parallel 3
ctest --test-dir build/linux-adaptive --parallel 2 --output-on-failure --output-junit <lane-result.xml>
python qt_review_lint.py --summary --diff=3b414f185ec9b6c23eb7fbfdbc0afc9bfed18b94
```

Qt plugin, QML import and library search paths point to the identified host Qt
installation. `QT_QUICK_BACKEND=software`,
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1`, and
`UBSAN_OPTIONS=halt_on_error=1` accompany native runs. Sanitizer coverage is
partial: selected test executables are instrumented; Qt, Hermes and every
static renderer object are not a wholly instrumented graph. Leak detection is
disabled for the Qt process allocation behavior. No whole-system memory-safety
claim follows from these results.

| Gate                                                             | Executed result                                              | Scope / limitation                                                                                                                                                                                                                                                                         |
| ---------------------------------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| All four experimental flags OFF                                  | **24/24 CTests passed**, 88.76 s; 0 failures, 0 skipped      | Full registered native host inventory, including renderer, Hermes, QNX Screen fake, the actual gallery, focus, fonts, metrics and sparse animation-ID regression.                                                                                                                          |
| All four experimental flags ON                                   | **40/40 CTests passed**, 209.58 s; 0 failures, 0 skipped     | Same frozen source plus desktop, input lifecycle, TextInput and accessibility experiments and their negative controls.                                                                                                                                                                     |
| Actual gallery, ordinary window                                  | Passed OFF (55.74 s) and ON (54.20 s)                        | Widths 360, 480, 800, 1024, 1280 and 1920 logical units; 1200×400 short viewport; six routes; controlled text, counter and switch; modal button/Escape, focus, native wheel, selection, scroll and item/caret retention. These are test-journey durations, not input-latency measurements. |
| Actual gallery, desktop window                                   | Passed ON (107.61 s)                                         | All routes and retained state, plus the true 480×360 outer minimum: **464×268 React client**. All six compact-menu destinations were reached through native scrolling/clicks; layout returned to a wider client.                                                                           |
| Density and text scale                                           | Passed in both flag configurations                           | Separate actual Qt runs at DPR 1, 1.25 and 2; host fontScale 1→1.5, native Text and empty TextInput, coherent consumer updates, duplicate notification and teardown checks. Physical cross-screen movement is not proven.                                                                  |
| JavaScript suite                                                 | **51 passed, 1 environment-blocked** of 52 discovered files  | Official runner exits 1 on launcher Unix-socket `listen EPERM`; all 37 subsequent files pass in the documented continuation. No full-suite pass is claimed.                                                                                                                                |
| Types, dependencies, manifests, architecture, packaging and docs | Passed focused checks                                        | Root and isolated consumer types; real local starter archives and Metro fixtures; frozen dependency policy; documented command/log scope in the UI receipt.                                                                                                                                |
| Qt C++ review                                                    | 0 final lint findings; 2 confirmed review findings corrected | 26 changed C++ files, 2,751 changed lines, two A1–A6 review shards and follow-up source checks. Runtime assertions for both fixes pass in the final matrix.                                                                                                                                |
| QNX cross-build                                                  | **Blocked / not run**                                        | Local licensed SDK and target Qt closure unavailable. Host QNX Screen fake is not a target compile.                                                                                                                                                                                        |
| QNX VM / physical hardware; Marathon/Wayland                     | **Not run**                                                  | No deployment, shell integration, physical HID, IME or assistive-technology qualification is inferred.                                                                                                                                                                                     |

Final repository-wide `pnpm format:check` and `pnpm validate:docs` both passed
after the evidence files were assembled. The dependency lock remains byte-for-byte
identical to the prerequisite.

### Retained evidence and native captures

- [Machine-readable flags, cases, commands, source and artifact hashes](adaptive-window-layouts-2026-11-09.json)
- [Combined native, build, JavaScript and development-failure logs](adaptive-window-layouts-2026-11-09.log)
- [OFF JUnit report](adaptive-window-layouts-native-off-2026-11-09.xml) and [ON JUnit report](adaptive-window-layouts-native-on-2026-11-09.xml)
- [JavaScript, public types and starter packaging receipt](adaptive-window-layouts-ui-validation-2026-11-09.md)
- [Consolidated Qt C++ source review](adaptive-window-layouts-qt-review-2026-11-09.md)

These are real Qt native captures from the final host run, not browser mockups or
physical QNX screenshots. Matching per-item geometry JSON and image hashes are
included beside each image.

| Capture                                                                 | Observed size                                                          |
| ----------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| [Wide gallery](adaptive-window-layouts-wide-host-2026-11-09.png)        | 1280×900 client, three columns                                         |
| [Compact gallery](adaptive-window-layouts-compact-host-2026-11-09.png)  | 480×700 client, single column and compact navigation                   |
| [Restored desktop](adaptive-window-layouts-desktop-host-2026-11-09.png) | 1100×760 outer window; 1084×668 client, two columns and retained state |
| [Desktop minimum](adaptive-window-layouts-minimum-host-2026-11-09.png)  | 480×360 outer window; 464×268 client and scrollable content/navigation |

![Native wide gallery with readable controls and three columns](adaptive-window-layouts-wide-host-2026-11-09.png)

## Test design and retained failures

The gallery test runs the production consumer main and real Metro app bundle.
It sends Qt window mouse, key and wheel events; it does not call JavaScript
handlers, inspect Zustand internals or directly set scroll offsets. It checks
native item identity and caret alongside visible React acknowledgements.
The upstream fail-closed wrapper, descriptor-derived identity, bundle/image/
executable hashes and rejection of JavaScript, image and sanitizer failures are
retained. A process returning zero cannot conceal those diagnostics.

During development, native tests exposed and led to corrections for:

- A focus outline intercepting modal pointer targets. It now paints with
  `enabled: false`, and consumer input is rerun after that change.
- Removing active Qt Tab eligibility before releasing focus. The bridge now
  releases focus first and skips unchanged policies.
- An assertion on the raw modal child count that included Qt-owned pointer
  decoration. The current prerequisite verifies actual React content and bounds.
- A stale/empty bundle base during native SourceCode initialization. The current
  prerequisite normalizes and publishes the source URL before evaluation; this
  branch preserves that repair and checks bootstrap-to-bundle initialization.
- A test racing the native wheel animation after a fixed 250 ms. It now waits
  for positive content offset and the end of movement within a bounded deadline.
- A duplicate-DPR assertion after listener removal. The listener remains active
  until native/runtime work drains and the unchanged-event assertion completes.

The first enabled-feature gallery sequence passed its ordinary-window lane in
48.84 seconds, but the expanded desktop sequence hit the inherited 75-second
outer process timeout after producing its minimum-window capture. The unified
driver adds substantially more resize and compact-navigation interactions. Its
process limit is now 150 seconds and the CTest limit 180 seconds, while each
interaction still has an 8-second bound. Progress output is flushed at every
step. This is a test workload budget, not a UI performance acceptance result.

The independent Qt review also caught sparse animation updates clearing native
identifiers. Presence-aware metadata and the adapter-host regression correct
that defect. The earlier failures and final reruns are retained separately;
none of the failed development runs is counted as a final pass. The broad JS launcher test is environment-blocked by Unix-socket
`listen EPERM`; no weakened assertion or skipped test is counted as a pass.

## Security, licensing and performance

No credentials, target addresses, SDK archives, Qt binaries, premium component
source, boot settings or privileged services are added. Code only observes its
own client window and uses the existing runtime dispatch path. Host tests use
synthetic data and input; no physical input claim is implied.

The two Roboto fonts are unmodified upstream bytes, pinned by SHA-256 in CMake,
under SIL Open Font License 1.1. Full provenance, the upstream license and
coverage limitations are in [`vnext/assets/fonts`](../../vnext/assets/fonts/README.md).
The embedded license is available through `--font-license` and installed as a
consumer license file. Added font data totals **1,019,528 bytes** before resource
or executable compression. The project's MIT license does not replace font,
Qt or QNX licensing. Existing Qt artifact provenance is not promoted to a new
public-source distribution qualification.

No FPS, latency, startup, memory or power improvement is claimed. Resize events
are coalesced without periodic idle polling; application layout work is bounded
by the current shell/grid and retained route set. There is no physical
resize-to-presentation percentile or comparable hardware baseline in this run.
Those measurements, text/input behavior on the actual QNX font engine, IME,
assistive technology, moving between real displays, system font preferences and
long-duration retention remain target qualification work.

## Merge gate, next slice and rollback

Keep the PR **draft and unmerged** until the new native executable and matching
bundle/assets are cross-built and exercised on the intended QNX target. The
repository's `AGENTS.md` and `qnx-platform-validation` skill require this because
host-only evidence cannot satisfy the target runtime gate. Updating only the
JavaScript bundle cannot deliver the native metrics, text or input changes.

The next smallest slice is physical target qualification of this exact native
candidate: normal and decorated windows, repeated resize, keyboard/pointer
navigation, text/caret and list retention, density/font fallback and own-window
captures. Marathon/Wayland and actual cross-display movement require separate
receipts. Provider work for system font preferences, keyboard insets and display
segmentation remains outside this compatibility repair.

Rollback by reverting this stacked PR's commits and rebuilding the native
executable plus matching bundle. The prerequisite #104 remains independently
reviewable. There are no database, persistent settings, firmware or boot
migrations to reverse. Optional desktop behavior can still be disabled through
its existing build flag and runtime launch option.
