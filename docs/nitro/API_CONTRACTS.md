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

## Native tree query for tests: `rnqnx::testing::waitForNode` (S3, proposed)

Pipeline status: step 1 (`api-design`) is this section. Step 2 (`build-nitro-modules`) produces no `.nitro.ts` spec and no Nitrogen or `nitro-platform-codegen.mjs` output, because the hook has no JavaScript surface: it is a C++ test-support library in `native/testing`, linked only into test executables, never into an app or runner. ADR-0004 makes an *app-facing* native API a HybridObject; this is not one. If a later slice exposes tree queries to JavaScript, it needs a `HybridObject` spec and a new contract here. Step 3 (`cpp`) is the per-file review note in the PR. No C++ exists until a lane other than the author has reviewed this section.

Skill sources, all at `margelo/react-native-skills@1e9e17be6f41c838db472d6a4b943fc26355a3be` (the head on 2026-11-20): `api-design` and `cpp` read in full and byte-identical to the local install; `build-nitro-modules` read from the pinned SHA (the local install carries an extra Nitro V1 paragraph that the pin lacks).

### Contract

```cpp
namespace rnqnx::testing {

struct NodeQuery {
  QString objectName;          // exact match, non-empty
  bool requireVisible = false; // the node's `visible` property must read true
};

struct NodeWaitResult {
  bool found = false;
  QPointer<QObject> node;               // set when found; null if the node died since
  std::chrono::milliseconds elapsed{0};
  QString error;                        // empty when found
};

// Synchronous scan. Same thread as `root`. Returns nullptr when absent.
QObject* findNode(QObject* root, const NodeQuery& query);

// Pumps the calling thread's event loop until the node exists or `timeout` passes.
NodeWaitResult waitForNode(QObject* root, const NodeQuery& query,
                           std::chrono::milliseconds timeout);
}
```

Roots and traversal: any `QObject`. The scan follows `QObject::children()`, `QQuickItem::childItems()` and the content item of a `QQuickWindow`, and visits each object once. The root itself can match.

### Call sites

```cpp
// 1. A node that appears after a deferred QML Loader finishes.
auto r = waitForNode(&window, {.objectName = "rnqnxTitleBar", .requireVisible = true}, 5s);
QVERIFY2(r.found, qPrintable(r.error));

// 2. Failure: the node never appears. The error names the node and the budget.
auto missing = waitForNode(&window, {.objectName = "noSuchNode"}, 200ms);
// missing.found == false
// missing.error == "timed out after 200 ms waiting for node 'noSuchNode' under QQuickWindow 'main' (scanned 14 objects)"

// 3. Cleanup: nothing to dispose. A wait holds no state after it returns.
```

### Error set

| Condition | Result |
| --- | --- |
| `root` null, `objectName` empty, `timeout` not positive | `std::invalid_argument` (a programming error, not a test failure) |
| Called off the thread that owns `root`, or from inside another `waitForNode` | `std::logic_error` |
| No `QCoreApplication` instance | `std::logic_error` |
| Timeout | `found == false`, `error` as above |
| `root` destroyed while waiting | `found == false`, `error` says "root destroyed while waiting for node '<name>'" |
| Node found but `requireVisible` is not met at the deadline | `found == false`, `error` says the node exists and is not visible |

### Waiting rules

- The wait is event-driven. An application-level event filter coalesces every delivered event into one zero-delay re-scan, and a single-shot timer enforces the deadline. There is no sleep, no fixed polling interval and no retry. A node that only appears because of a posted or queued event is seen on the next event turn.
- The filter and the timers live on the stack of one `waitForNode` call (RAII). They are removed on every exit path, so no callback runs after the function returns.
- No lock is taken and no user code runs while the scan walks the tree. The scan reads `objectName()` and the `visible` property only.
- Cancellation: there is none beyond the deadline and root destruction. Tests that need a longer budget pass a longer `timeout`.

### What each platform promises

| Platform | Promise |
| --- | --- |
| Host (macOS, Linux, Qt 6.11+) | Built and run by the `native-testing` CTest lane under `QT_QPA_PLATFORM=offscreen`. |
| QNX VM, Pi 400, release board | Not built. The library is not linked into any QNX target in this slice. Device smoke paths wait on app-emitted log lines through `scripts/lib/wait-log.sh`. |

The accessibility tree (`QAccessible`) is not queried. The `objectName` tree covers every node the smoke paths need, and the offscreen platform does not install an accessibility bridge, so an accessibility query would not run in the host lane.
