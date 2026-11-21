# Nitro v1 API contracts and platform-native lifecycle

Reviewed 2026-11-07. This is a **migration/proof API**, not a claim of general QNX or Marathon Nitro 1.0 support.

## Source provenance and status

- `react-native-qnx`: QNX Nitro v0.37.1 originally implements a native synchronous `QnxPlatformInfo` and a Hermes JSI probe. Only historical Screen/host checks have run, not the upgraded Nitro v1 on a QNX Pi.
- `margelo/nitro` source tag: `v1.0.0-alpha.nightly.136.gdb81de61bd32` (reviewed source, npm artifacts still must be verified). Audited on 105; re-pinned to 136 with `Dispatcher.hpp`/`Callback.hpp` byte-identical; QNX cross-build and device not re-run. In v1 the public runtime entry moves into `nitro-core`, registration uses `HybridClassRegistry`, and JS obtains a constructor with `NitroModules.getHybridClass()`.
- Nitro 1.0 is **alpha**. Explicitly reject calling the migration production-ready until frozen lockfile, C++ compiler, Hermes JSI and physical runtime evidence are captured.
- The inspected v1 Nitrogen source `packages/nitrogen/src/getPlatformSpecs.ts` presently uses `platformSchema = z.enum(['apple', 'android'])`. QNX and Marathon codegen are not there today. Do not falsify generated binding output. Hand-written `QnxPlatformInfo` is an **experimental native registration probe** and an intentionally narrow exception, not the finished public Nitro module strategy.

## User-facing TypeScript contract: existing proof

```ts
import { createQnxPlatformInfo } from '@rn-qnx/qnx-nitro-runtime';

const info = createQnxPlatformInfo();
info.platform; // 'qnx', guaranteed by this QNX-only API
info.architecture; // e.g. 'aarch64', determined natively
const answer: number = info.add(20, 22); // 42, synchronously
```

The real JS wrapper imports `NitroModules` from `nitro-core`, resolves `getHybridClass('QnxPlatformInfo')`, then invokes `new`. This import MUST happen after the native installer ran on the same Hermes runtime. The import can throw if no native Nitro proxy is available: that is an installation failure, not a condition to mask.

**Unsupported required capability should throw**, not return `undefined` or a misleading success. No implicit fallback to a TurboModule, Android runtime or QNX Screen probe.

## Example 2: native use during QNX JSI runtime probe

```cpp
auto runtime = facebook::hermes::makeHermesRuntime();
rnqnx::nitro::installQnxNitroRuntime(*runtime);
// Evaluate JS: new NitroModulesProxy.getHybridClass(...) is invalid syntax;
// first bind the constructor to a variable, then instantiate:
```

```js
const PlatformInfo = NitroModulesProxy.getHybridClass('QnxPlatformInfo');
const info = new PlatformInfo();
if (info.add(20, 22) !== 42) throw new Error('Nitro binding returned incorrect result');
```

Test again after native module teardown; observe runtime creation, registry idempotency, class-prototype materialization, JS errors and leaks. Do not access `NitroModulesProxy` before native `install(runtime)`.

## Proposed later native public API — DO NOT expose until implemented

A portable API should be a source-generated, QNX/Marathon-capability-aware HybridObject from a real `.nitro.ts` spec, not a raw C++/JSI method count and not a Platform.OS spoof:

```ts
type NativePlatform = 'qnx' | 'marathon';
type NativeFeature =
  'text-input' | 'safe-area' | 'notifications' | 'background-tasks' | 'media' | 'bluetooth' | 'gpu';

interface NativePlatformCapabilities {
  readonly platform: NativePlatform;
  readonly architecture: string;
  hasFeature(feature: NativeFeature): boolean;
}
```

This is an _API sketch_, not an executable `.nitro.ts` file: it intentionally adds no unsupported generated fields and no implementation promises. Each platform adapter must return **accurate** capability values from permissions/OS support and handle mandatory failures explicitly. A shell-owned permission is not a generic available feature. For third-party modules prefer their upstream generated Nitro specs and typed exports when the v1 generator supports the target.

## API design review — decisions made

1. A synchronous, pure computation `add(left,right)` stays synchronous; do not add a Promise or JS queue for a trivial scalar.
2. The QNX-only info object remains small, typed and explicit; do not grow it into a platform singleton containing networking, Bluetooth, input, and graphics. Split future HybridObjects by lifecycle/capability.
3. No fake async dispatcher. QNX/Marathon `QObject`/Qt event dispatch needs ownership, cancellation and runtime-destruction tests before enabling callbacks or Nitro Promise methods.
4. No global mutable fake platform constants. Platform fields must come from the actual native host; missing installer fails early.
5. No manual editing generated Nitro artifacts. For proper QNX and Marathon codegen, extend Nitrogen with source-reviewed platform targets, emit and compile spec-generated C++ bindings, run typecheck, exercise native APIs, and only then publish.
6. No claims that `react-native-nitro-fetch`, `react-native-mmkv`, VisionCamera or other iOS/Android Nitro modules run on QNX/Marathon. Every module needs its own platform implementation and confidence evidence.
7. No giant zero-copy payload eager JS conversion; hold large native resources behind stateful HybridObjects with explicit cleanup and lazy `ArrayBuffer` access.
8. Any listener API must use additive `addOn...Listener` with an idempotent remove/cleanup handle, or document a true single-owner setter. Permission revocation and native destroy must stop further native callbacks.
9. Before adding an external API use concrete docs and actual RN 0.88/Expo 58/musl/QNX headers; don't hand-wave "API compatibility" on stable npm tags.

## Test matrix / proof

| Gate                   | Test class        | Pass criterion                                                                                         | Current evidence                       |
| ---------------------- | ----------------- | ------------------------------------------------------------------------------------------------------ | -------------------------------------- |
| v1 upstream API        | source            | `HybridClassRegistry`, descriptor, constructor and `getHybridClass` align with inspected pinned source | source inspected                       |
| QNX file include paths | source            | `NitroModules/*.hpp` resolves to `nitro-core/native/cpp/include`                                       | source changes proposed                |
| JS contract            | unit / TypeScript | exported wrapper and `index.d.ts` match `new QnxPlatformInfo`, no v0 calls                             | new static checks added                |
| JS installation        | Hermes native     | proxy installed first, missing installer throws, repeated registration safe                            | **not run**                            |
| Thread & lifetime      | C++ ASan/TSan     | no callbacks after runtime destroyed; async unsupported fails clearly                                  | **not run**                            |
| Typed codegen          | Nitrogen          | generated .nitro.ts, QNX/Marathon native backend compiled                                              | **blocked on upstream target support** |
| QNX hardware           | SDP 8.0.5 Pi4/5   | signed/offline reproducible binary; arch/add assertions; 3 clean runs                                  | **not run**                            |
| Marathon hardware      | Linux/Wayland     | a separate Linux build; no QNX ELF reuse; QML/Wayland integration                                      | **not run**                            |
| Consumer app           | Expo/Metro        | published-package-like install, no private workspace assumptions                                       | **not run**                            |

Track exact failures and commit evidence. Code and dependency changes remain draft until genuine native and QNX/Marathon tests are green.

## Skills order

For every public Nitro API change: `api-design` first; `build-nitro-modules` second; `cpp`/language skill for owned native implementation; `react-native-best-practices` for profiling; `react-native-testing` for tests; `github-actions` for CI. For networking or persistent storage specifically, consult `nitro-fetch` or `react-native-mmkv` skill **only if that API is in scope**. See `SKILLS_AND_ROSTER.md`.

## Additional non-QNX compile gate

`tests/native/nitro-v1-host/CMakeLists.txt` and `.github/workflows/nitro-v1-host.yml` compile the **actual** `nitro-core@1.0-alpha` C++ source and the changed `QnxPlatformInfo` bridge on Linux with C++20. A passing compile verifies C++ headers, templates and source-object compatibility; it does not verify QNX SDP linking, runtime JSI correctness, Fabric/Qt or physical device support. Do not turn compiler failures into ignored warnings or substitute a standalone fake class.

## Physical Pi 400 synchronous proof (2026-11-07)

The existing QNX class now cross-builds with SDP 8.0.5 and runs inside the
actual React Native/Fabric/Hermes/Qt Quick Hello World app. Its v1
`getHybridClass()` constructor, two getters and `add(20, 22)` passed as
`qnxuser`, followed by a verified 640×480 rendered capture and exit zero.
See [device evidence](../evidence/pi400-hello-world-2026-11-07.txt).
Official npm SHA-512 checks and complete installed-file comparisons passed
for core (333 files), RN adapter (280), and Nitrogen (608), all locked to
nightly 105 (device evidence for alpha 105 only; not re-run on 136). This supersedes the older no-physical-run statement only for
this synchronous proof. The later opt-in [Qt dispatcher](QT_DISPATCHER.md)
also has physical callback/Promise/checkpoint/teardown evidence. General QNX
codegen and third-party Nitro modules remain unverified. The normal upstream RN
TurboModule startup plumbing remains; Nitro serves this app-native proof.

## S4 frame and startup timing probe (internal, no JavaScript API)

Added 2026-11-20 for the `performance` gate. This section is the `api-design`
output (Margelo skill, step 1) and the `build-nitro-modules` decision (step 2)
for the timing probe. An independent review (`ecc:code-reviewer`, a lane other
than the author) ran on the first draft before any C++ was written and asked
for the changes now folded in; its record goes in the S4 receipt.

### Decision: no HybridObject, no `.nitro.ts` spec

The probe measures the native host from outside the React tree. Nothing in
JavaScript needs to start it, read it or stop it. Plan 03 keeps tracing
internal ("no general application-facing Nitro performance API is required for
the first slice"), and the pinned `api-design` skill says to keep
implementation strategy out of the public shape. `build-nitro-modules` applies
only where a HybridObject is exposed. So:

- There is no new `.nitro.ts`, no entry in `scripts/nitro-platform-codegen.mjs`
  and no generated or hand-written Nitrogen-looking output.
- The surface is a native C++ class used by the playground host and a few
  command line options.
- If a later slice needs JavaScript-visible timing, it starts again at step 1
  with a typed spec, and this section stops being the contract.

Why not extend `RuntimeTrace` (`RNQNX_ENABLE_FRAME_TRACE`): its `record()` takes
a mutex while copying a record, and Plan 03 asks for no mutex on a callback
path. `RuntimeTrace` also has no Qt render-loop stage and its schema has no
frame field. The timing probe is a separate, smaller recorder with one writer
and no lock. It does not implement Plan 03's thread identity or its ON/OFF
overhead gate: no overhead comparison has been measured, so budgets recorded
with the probe on carry that caveat, and the probe is default OFF.

### Public contract

Command line, accepted only when the host is built with
`RNQNX_ENABLE_TIMING_PROBE=ON` (default OFF; CMake rejects it on QNX and on
any system other than Linux or Darwin):

| Option                       | Meaning                                                                                                                                                     |
| ---------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `--timing-json <path>`       | Run the bundle, record, write a new JSON file and exit. The path must not exist; this is checked at option parse, before the workload, and again at export. |
| `--timing-settle-ms <n>`     | Milliseconds to keep running after the first frame before the wheel script. Default 500, range 0 to 60000.                                                  |
| `--timing-wheel-notches <n>` | Post `n` wheel notches to the window after settling, one every `--timing-wheel-gap-ms`. Default 0, range 0 to 1000.                                         |
| `--timing-wheel-gap-ms <n>`  | Gap between notches. Default 200, range 1 to 5000.                                                                                                          |

`--timing-json` requires `--bundle` and is rejected together with `--screenshot`
with `--replace`, `--restore-only` and `--stop-running`, and with `--dev-server` (the dev path wraps the run in a reload loop, which has
no single origin). Any value outside its range, a non-integer value, or a
timing option without `--timing-json` exits 2 with
`RNQNX_TIMING_CONFIG_ERROR <reason>` on stderr and runs no workload.

C++ (`vnext/probes/playground/TimingProbe.h`), namespace `rnqnx::probe`:

```cpp
struct TimingConfig {
  int settleMs, wheelNotches, wheelGapMs;
  int frameCapacity;  // internal, 4096; tests lower it to exercise overflow
  // Empty string keeps the default. Returns nullopt and sets `error` on a bad value.
  static std::optional<TimingConfig> fromStrings(const QString& settleMs,
      const QString& wheelNotches, const QString& wheelGapMs, QString* error);
};

class TimingProbe final : public QObject {
 public:
  // `origin` is the steady_clock reading main() takes as its first statement.
  // Connects to `window` with Qt::DirectConnection before the window is shown.
  TimingProbe(QQuickWindow& window, TimingConfig config,
              std::chrono::steady_clock::time_point origin);
  ~TimingProbe() override;              // disconnects; safe at any point after construction
  TimingProbe(const TimingProbe&) = delete;
  TimingProbe& operator=(const TimingProbe&) = delete;

  void markBundleEvaluated() noexcept;  // GUI thread, once
  void markSurfaceStarted() noexcept;   // GUI thread, once, after host.startSurface returns
  // Waits for the first swap after markSurfaceStarted, settles, posts the wheel
  // script, snapshots, then calls `done` once on the GUI thread. done(false)
  // means no swap arrived within 30 s of start().
  void start(std::function<void(bool)> done);
  // Valid after done(true). Returns an error string, empty on success.
  QString writeJson(const QString& path) const;
};
```

Semantics:

- The origin is the `steady_clock` reading `main()` takes as its first
  statement, before `QGuiApplication` exists. It excludes dynamic loading before
  `main()`. The JSON says so in `scope`.
- `firstFrameMs` is the first `QQuickWindow::frameSwapped` delivered after
  `markSurfaceStarted()`. A swap of the empty window before the surface starts
  is stored in `frameMs` but is not `firstFrameMs`. `surfaceStartedMs` and
  `bundleEvaluatedMs` are recorded separately.
- A frame record is the `steady_clock` reading taken inside the signal handler,
  in nanoseconds from the origin. The connection is `Qt::DirectConnection`, so
  the reading is taken on the thread that emits (the render thread under the
  threaded loop, the GUI thread under the basic and software loops). It is
  not a display presentation timestamp and no JSON field claims it is. Whether
  the offscreen platform and software backend emit `frameSwapped` the same way
  as a windowed GL backend is not assumed: the JSON records `platform` and
  `renderBackend`, and the S4 receipt states what the host run showed.
- Only the app window is connected. A second window (for example the LogBox
  surface) is excluded.
- Storage is one fixed array of 4096 slots allocated at construction, sized for
  the workloads in this slice (500 ms settle plus 40 notches at 200 ms is about
  8.5 s, a few hundred frames), not as a general bound. There is one writer,
  the signal handler. It does `n = total.load(relaxed)`, stores the slot when
  `n < 4096`, then `total.store(n + 1, release)`. There is no read-modify-write
  and no lock, allocation, file write or formatting on that path.
- The handler and its storage live in a `shared_ptr<State>` captured by the
  connection functor. `~TimingProbe` disconnects and sets `State::stopped`
  (release); the handler checks it first. A handler already running on the
  render thread therefore never touches freed memory, whatever the destruction
  order of the probe and the window.
- At `done` the probe loads `total` once with acquire order. `storedFrames` is
  `min(total, 4096)` and `droppedFrames` is `total - storedFrames`, both from
  that one load. Swaps after `done` are ignored by the export. A capture with
  `droppedFrames > 0` is invalid for budgets and the validator rejects it.
- RSS is read once, after the wheel script ends. `rssKiB` is the resident set
  and `peakRssKiB` is the peak, both from the OS (Linux `/proc/self/status`
  `VmRSS` and `VmHWM`; macOS `task_info` `resident_size` and `getrusage`
  `ru_maxrss`, which macOS reports in bytes, divided by 1024). Nothing is
  estimated.
- The wheel script uses `QCoreApplication::postEvent`, not `sendEvent`, so it
  does not run inside the timer callback that schedules it.
- The probe leaves the existing 16 ms microtask `drainTimer` in place. Timing
  runs include its quantisation; it is part of what is measured, and the JSON
  does not hide it.

Error set (every failure exits nonzero and writes no JSON file):

| Condition                                                                        | Exit | Message                                            |
| -------------------------------------------------------------------------------- | ---- | -------------------------------------------------- |
| option out of range or malformed, or a rejected combination                      | 2    | `RNQNX_TIMING_CONFIG_ERROR`                        |
| output path exists at parse time                                                 | 2    | `RNQNX_TIMING_CONFIG_ERROR`                        |
| no frame swapped after surface start within 30 s of `start()`                    | 1    | `RNQNX_TIMING_NO_FRAME`                            |
| bundle or surface start failed, or `applicationFailure()` non-empty at any check | 1    | the existing `RNQNX_PLAYGROUND_*_FAILED` lines     |
| `stopSurface` or its drain reports a failure after the run                       | 1    | the existing `RNQNX_PLAYGROUND_RENDER_FAILED` line |
| output path appears before export, or write fails                                | 1    | `RNQNX_TIMING_EXPORT_FAILED`                       |

The run tears down like the screenshot path: after `done`, `stopSurface`, drain
20 times, recheck `applicationFailure()`, then export. The output directory must exist and be writable, checked at parse time. The export writes a
temporary file in the destination directory and links it into place without
overwriting, so a failed write leaves nothing at the destination. The file has mode 0600. SIGINT and
SIGTERM end the process without a JSON file.

Disposal and cancellation: `TimingProbe` is stack-owned by `main`. It is
destroyed after the host and before the window, which is safe because it never
calls into the host and no event loop runs in between. The 30 s watchdog is a single-shot `QTimer`
owned by the probe. There is no cancel call.

JSON (`schema` 1) fields: `schema`, `scope`, `clock`, `platform`
(`QGuiApplication::platformName()`), `renderBackend`, `os`, `bundleEvaluatedMs`,
`surfaceStartedMs`, `firstFrameMs`, `frameMs` (every stored record, milliseconds
from origin), `totalFrames`, `droppedFrames`, `wheelNotchesSent`, `wheelFirstMs` and
`wheelLastMs` (when each notch was posted; `null` without a script), `rssKiB`,
`peakRssKiB`, `rssSource`. Intervals, percentiles and budget comparison are
computed outside the host by `scripts/validate-budgets.mjs` (added in this
slice) from `frameMs`, so the host records no derived statistic that could hide
a dropped record.

### Call sites

```sh
# Hello World, startup and RSS only
rnqnx_playground_host --bundle hello-world.bundle.js --timing-json /tmp/hello.json

# 1000-row list, 40 wheel notches 200 ms apart after a 500 ms settle
rnqnx_playground_host --bundle list-1000.bundle.js --timing-json /tmp/list.json \
  --timing-wheel-notches 40

# Failure: the output file exists
rnqnx_playground_host --bundle hello-world.bundle.js --timing-json /tmp/hello.json
# stderr: RNQNX_TIMING_CONFIG_ERROR output exists; exit 2; no workload ran
```

### What each platform promises

| Platform                                   | Promise                                                                                                                                                                                                                                        |
| ------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Host (Linux, macOS), offscreen or windowed | Startup to the first swap after surface start, stored swap instants, RSS. Labelled `host`.                                                                                                                                                     |
| QNX Screen (VM, Pi 400, release board)     | None in this slice. CMake rejects `RNQNX_ENABLE_TIMING_PROBE=ON` when `CMAKE_SYSTEM_NAME` is `QNX`. The device and VM budget fields stay `unmeasured` until a QNX reader and clock mapping are written, reviewed and run on a QNX target (S9). |
| Marathon                                   | Not in scope (frozen).                                                                                                                                                                                                                         |

## Native tree query for tests: `rnqnx::testing::waitForNode` (S3)

Pipeline status: step 1 (`api-design`) is this section. Step 2 (`build-nitro-modules`) produces no `.nitro.ts` spec and no Nitrogen or `nitro-platform-codegen.mjs` output, because the hook has no JavaScript surface: it is a C++ test-support library in `native/testing`, linked only into test executables, never into an app or runner. ADR-0004 makes an _app-facing_ native API a HybridObject; this is not one. If a later slice exposes tree queries to JavaScript, it needs a `HybridObject` spec and a new contract here. Step 3 (`cpp`) is the per-file review note in the PR.

Review: `ecc:cpp-reviewer` (a lane other than the author) reviewed the first draft before any C++ existed and returned "approve with changes". This text carries those changes: the application event filter is replaced by `QAbstractEventDispatcher::aboutToBlock()` (the filter saw its own timer events and spun, and only reaches main-thread objects), designated initializers are gone (the repo builds C++17 with `-Wpedantic -Werror`), the result gained a status enum, and matching, visibility and reentrancy are defined.

Skill sources, all at `margelo/react-native-skills@1e9e17be6f41c838db472d6a4b943fc26355a3be` (the head on 2026-11-20): `api-design` and `cpp` read in full and byte-identical to the local install; `build-nitro-modules` read from the pinned SHA (the local install carries an extra Nitro V1 paragraph that the pin lacks).

### Contract

```cpp
namespace rnqnx::testing {

struct NodeQuery {
  QString objectName;          // exact match, non-empty
  bool requireVisible = false; // QQuickItem::isVisible() / QWindow::isVisible()
};

enum class NodeWaitStatus { Found, TimedOut, NotVisible, RootDestroyed, AppQuit };

struct NodeWaitResult {
  NodeWaitStatus status = NodeWaitStatus::TimedOut;
  bool found() const { return status == NodeWaitStatus::Found; }
  QPointer<QObject> node;               // set when found; may be null again by the time the caller reads it
  std::chrono::milliseconds elapsed{0}; // steady clock
  QString error;                        // empty when found; names the node, the budget and the root type
};

// One synchronous scan. `root` must live in the calling thread. nullptr when absent.
QObject* findNode(QObject* root, const NodeQuery& query);

// Scans now, then pumps the calling thread's event loop until the node exists or
// `timeout` passes. `root` must live in the calling thread.
NodeWaitResult waitForNode(QObject* root, const NodeQuery& query,
                           std::chrono::milliseconds timeout);
}
```

Matching: the first object in pre-order that satisfies every field of the query. At each object, `QObject::children()` come before `QQuickItem::childItems()`. Each object is visited once. The root can match. `requireVisible` reads `QQuickItem::isVisible()` or `QWindow::isVisible()` by dynamic type, so the scan runs no user code. A name match on any other type is not a match, so a later sibling can still satisfy the query. When nothing matches and such a type was seen, the call throws `std::logic_error` naming the class. That throw happens after the event loop ends, never inside a dispatcher slot. A `QQuickWindow` is walked through its `contentItem` as well. A visible node does not imply a nonzero size, opacity or an exposed window. The scan does not reach the content of a closed `Popup` and does not follow `QWindow` children except through `QObject::children()`.

### Call sites

```cpp
using namespace std::chrono_literals;
namespace t = rnqnx::testing;

// 1. A node that appears after a deferred QML Loader finishes.
t::NodeQuery bar;
bar.objectName = QStringLiteral("rnqnxTitleBar");
bar.requireVisible = true;
const t::NodeWaitResult r = t::waitForNode(&window, bar, 5s);
QVERIFY2(r.found(), qPrintable(r.error));

// 2. Failure: the node never appears. The status is TimedOut and the error names
//    the node, the budget and the root type, for example
//    "timed out after 200 ms waiting for node 'noSuchNode' under QQuickWindow 'main'".
t::NodeQuery missing;
missing.objectName = QStringLiteral("noSuchNode");
const t::NodeWaitResult gone = t::waitForNode(&window, missing, 200ms);
QCOMPARE(gone.status, t::NodeWaitStatus::TimedOut);

// 3. A negative assertion with the synchronous scan, after a node was removed.
QVERIFY(t::findNode(&window, bar) == nullptr);
```

### Error set

| Condition                                                                                                                                                          | Result                                                             |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------ |
| `root` null, `objectName` empty, `timeout` not positive                                                                                                            | `std::invalid_argument` (a programming error, not a test failure)  |
| `root` lives in another thread, a nested `waitForNode` (detected with a `thread_local` flag), no `QCoreApplication`, `requireVisible` on a type with no visibility | `std::logic_error` naming the call                                 |
| Timeout                                                                                                                                                            | status `TimedOut`, error as above                                  |
| Node found but not visible at the deadline                                                                                                                         | status `NotVisible`, error says the node exists and is not visible |
| `root` destroyed while waiting                                                                                                                                     | status `RootDestroyed`                                             |
| `QCoreApplication::quit()` while waiting                                                                                                                           | status `AppQuit`                                                   |

Callers branch on `status`. The `error` string is for people.

### Waiting rules

- The wait is event-driven and runs a nested `QEventLoop` on the root's thread. The first scan is synchronous, before the loop starts. After that it rescans each time the thread's event dispatcher reports `aboutToBlock()`, and once more at the deadline. One single-shot timer enforces the deadline, measured on a steady clock. There is no sleep, no polling interval, no retry and no application-level event filter.
- The wait makes no progress of its own. It does not show, expose, polish or render anything, and it cannot see a change that is not made by a handler running on the root's thread. A test that needs a window exposed exposes it first. A node created by an asynchronous incubator on a window that never renders can stay absent until the deadline.
- Other nested loops (`QTest::qWait`, a dialog `exec`, a QML handler) may run while the wait is active. Only a nested `waitForNode` is refused.
- The loop, the timer and the dispatcher connection live on the stack of one call. Destruction removes the connection and the timer before the loop, on every exit path including an exception, so nothing runs after the function returns. The root is held as a `QPointer` and re-checked on every scan.
- No mutex exists. Nothing is shared between threads. The scan runs inside a dispatcher signal and only reads `objectName()`, the child lists and visibility.

### Build and linkage

The library is `rnqnx_native_testing` in `native/testing`: static, built only under `RNQNX_BUILD_NATIVE_TESTING_TESTS`, never installed or exported. It links Qt6::Core, Gui and Qml publicly and Qt6::Quick privately, uses C++17 and `QT_NO_KEYWORDS` like the window chrome lane, and builds with `-Wall -Wextra -Wpedantic -Werror` on Clang and GCC. A CTest check fails if a target other than a test links it.

### What each platform promises

| Platform                      | Promise                                                                                                                                                     |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Host (macOS, Linux, Qt 6.11+) | Built and run by the `native-testing` CTest lane under `QT_QPA_PLATFORM=offscreen`.                                                                         |
| QNX VM, Pi 400, release board | Not built. The library is not linked into any QNX target in this slice. Device smoke paths wait on app-emitted log lines through `scripts/lib/wait-log.sh`. |

The accessibility tree (`QAccessible`) is not queried. The `objectName` tree covers every node the smoke paths need, and the offscreen platform does not install an accessibility bridge, so an accessibility query would not run in the host lane.
