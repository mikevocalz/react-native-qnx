# Nitro v1 API contracts and platform-native lifecycle

Reviewed 2026-11-02. This is a **migration/proof API**, not a claim of general QNX or Marathon Nitro 1.0 support.

## Source provenance and status

- `react-native-qnx`: QNX Nitro v0.37.1 originally implements a native synchronous `QnxPlatformInfo` and a Hermes JSI probe. Only historical Screen/host checks have run, not the upgraded Nitro v1 on a QNX Pi.
- `margelo/nitro` source tag: `v1.0.0-alpha.nightly.105.g69d365fc9b18` (reviewed source, npm artifacts still must be verified). In v1 the public runtime entry moves into `nitro-core`, registration uses `HybridClassRegistry`, and JS obtains a constructor with `NitroModules.getHybridClass()`.
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
