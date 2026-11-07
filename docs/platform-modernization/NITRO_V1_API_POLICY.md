# Nitro v1 API-first implementation contract

The owner explicitly requires this program to use **Nitro v1's new APIs** and API-first design for modern, efficient native interfaces. This is an implementation gate for every application-facing native workstream, including lifecycle, services, buffers, device metrics and Forge. It is not a request to wrap legacy calls in a Promise or introduce a parallel bridge.

## Actual source baseline

The publication base is `48e37f7550b4244f8a7936c01af75419e1fec416`; the source links below retain the inspected implementation revision `62d530bcd8cea578f59bb0d693e0402a5cadefde`. Its installed graph pins `nitro-core`, `react-native-nitro-modules` and `nitrogen` to `1.0.0-alpha.nightly.105.g69d365fc9b18`. Those are audited prerelease dependencies. A later v1 revision may be selected only through an explicit compatibility change with matching core, adapter, generator, lockfile, native headers and runtime tests.

The following are actual repository sources, not inferred v1 syntax:

- [PlatformInfo.nitro.ts](https://github.com/mikevocalz/react-native-qnx/blob/62d530bcd8cea578f59bb0d693e0402a5cadefde/packages/qnx-nitro-runtime/specs/PlatformInfo.nitro.ts) imports `HybridObject` from `nitro-core` and declares `export declare class PlatformInfo extends HybridObject` with typed getters and `add`.
- [JavaScript entry](https://github.com/mikevocalz/react-native-qnx/blob/62d530bcd8cea578f59bb0d693e0402a5cadefde/packages/qnx-nitro-runtime/index.js) imports `NitroModules` from `nitro-core`, resolves `NitroModules.getHybridClass('QnxPlatformInfo')`, and invokes the returned constructor with `new` after installation.
- [Native installation](https://github.com/mikevocalz/react-native-qnx/blob/62d530bcd8cea578f59bb0d693e0402a5cadefde/packages/qnx-nitro-runtime/cpp/QnxNitroRuntime.cpp) registers the immutable descriptor through `HybridClassRegistry::registerHybridClass`, installs Nitro into the same Hermes runtime and, for the async overload, installs the owned runtime dispatcher.
- [Generated binding adapter](https://github.com/mikevocalz/react-native-qnx/blob/62d530bcd8cea578f59bb0d693e0402a5cadefde/scripts/nitro-platform-codegen.mjs) emits `HybridClass` constructor/property/method registration for the constrained PlatformInfo contract. Its parser deliberately rejects unsupported shapes.
- [Owned async contract](https://github.com/mikevocalz/react-native-qnx/blob/62d530bcd8cea578f59bb0d693e0402a5cadefde/docs/nitro/QT_DISPATCHER.md) supplies the existing Promise/callback completion path, checkpoints and teardown rules. It does not prove general QNX codegen or every third-party module.

An interface sketch in a plan describes user-visible types and semantics. It is **not** a v0 Nitro spec, generated output, or proof that the richer v1 shape already works. Convert it into the selected v1 class declaration and supported target metadata only after inspecting actual generator/runtime source. Do not invent a QNX decorator or mark the existing Apple/Android metadata as QNX support.

## Mandatory API workflow

1. Read the pinned Margelo `api-design` skill, then `build-nitro-modules`, then `cpp`, and record their applied decisions. Read the repository's `qnx-native-api` skill. Generic older examples in those sources must be adapted to the actual v1 source above.
2. Specify the user's operations, units, capability availability, errors, cancellation, timeout, resource lifetime, observations and cleanup. Provide two or three realistic calls that include failure and disposal before writing bindings.
3. Audit every required v1 type shape: returned resource classes, Promise values and void, callbacks, error conversion, arrays/unions/optionals, buffers, inheritance, constructors and external-memory accounting. Extend supported generation with deterministic output and negative tests; do not hand-edit generated files.
4. Implement the generated/native contract directly. Keep the public imperative API aligned with it. React hooks and components may compose that API, but must not quietly normalize away failed operations, change ownership, or mask unsupported behavior.
5. Prove the API in the real Hermes runtime and Qt/Fabric application. Typechecking, source generation, a native archive or a JavaScript import alone cannot establish the feature.

The existing constrained proof adapter remains useful. A hand-authored v1 experimental binding is permissible only as an explicitly labeled, isolated capability experiment with the missing generator work tracked; it cannot be presented as the finished public module or general platform codegen. The delivery target is supported, reproducible v1 binding generation plus real API behavior.

## Efficiency and semantics

| API concern                             | Required design                                                                                                                                                                  |
| --------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Cheap local observation                 | Use a typed, cached synchronous property or small pure method. Do not add Promise/queue overhead without a real async boundary.                                                  |
| IPC, I/O, hardware setup, heavy compute | Use an owned native execution context and asynchronous completion. A JS Promise around a synchronous blocking native call does not move that work off the JS/GUI owner.          |
| Native state                            | Return a ready resource/session from a factory. Keep one owner route and define disposal; avoid giant singleton objects and partially initialized handles.                       |
| Repeated observations                   | Use additive listeners with owned, idempotent cleanup and an ordered initial snapshot. Keep generation and sequence bookkeeping internal unless it is part of the user's domain. |
| Failure                                 | Surface real Error failures, explicit unsupported required capabilities and predictable post-disposal behavior. Keep serialized error envelopes inside transport boundaries.     |
| Bulk data                               | Retain native ownership and expose deliberate copying or resource operations. Do not eagerly convert a large payload or claim zero-copy across unmeasured boundaries.            |
| Threading                               | Reuse the existing completion queue and RuntimeScheduler. Keep heavy native work on its owner; preserve Qt/JS affinity and avoid nested executor chains.                         |
| Shutdown                                | Stop producers and account for accepted operations before host drain. Drain, cancellation, timeout and server-side-effect cancellation are distinct contracts.                   |

Measure actual interaction latency, native work, allocation and copy counts before claiming an efficiency gain. Proposed Plan03 must define the measurement lane; it does not prescribe a universal millisecond budget or RT priority.

## Forge and third-party boundaries

Proposed Plan10 must first prove reusable package discovery/linking/lifecycle hooks using a non-premium fixture. Forge remains a separately licensed product: its production bindings and configuration belong with Forge or its consuming app, and its tests are optional and separately authorized. Core builds, conformance and releases must work with Forge absent. The optional compatibility lane must align Forge's audited alpha120 binding contract with the runtime's alpha105 baseline, either by adapting/regenerating the bindings or by a separately validated v1 runtime upgrade. Preserve the actual Factory/Document API and its documented typed-outer/JSON-payload limitation until a richer generator/engine contract is proven. Choose one worker owner instead of stacking a second pool around the QNX host.

RN core's existing internal TurboModule registry contracts remain framework integration details. They do not authorize old application-facing APIs. Worklets, Reanimated and RNGH must use their audited real bootstrap contracts through the separately reviewed architecture decision in Plan 02; disguising them as a Nitro facade would not implement those libraries.

## Acceptance and review

The native API reviewer must check actual v1 imports, declarations, registration, constructor behavior, exact ABI alignment, generated signatures and dispatcher installation order. Behavior tests cover success and invalid input, denied/unavailable service, two independent consumers, callback failure, cancellation races, stale generations, repeated disposal, reload and accepted work during shutdown. Include missing-installer and unsupported-generation negative cases.

Use source-contract checks to prevent accidentally reintroducing v0-only constructor registration or `createHybridObject` patterns into the new v1 application path, while keeping intentional historical migration fixtures labeled. Do not substitute those checks for native execution. Keep QNX and Marathon result rows separate and record which exact v1 revision ran.

API architect and Nitro/codegen engineer are proposed implementation DRIs; a different C++/JSI lifetime reviewer and target regression reviewer must assess the finished slice. Human assignments remain unassigned. Reverting a proposed API requires disabling its experiment and restoring matching package, generated/native, bundle and persisted-format compatibility together.
