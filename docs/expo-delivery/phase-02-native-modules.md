# Phase 2: supported native packages install and execute without hand wiring

Status: proposed delivery plan; documentation only. Source baseline is main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, inspected 2026-11-07.
Implementation predecessors: [Phase0](https://github.com/mikevocalz/react-native-qnx/pull/81) baseline and [Phase1](https://github.com/mikevocalz/react-native-qnx/pull/82) external-app/native-host contract. Audit and public API design can begin before Phase1 device completion.

## Outcome and source boundaries

For a supported package, deliver install -> discover -> generate -> compile -> link -> register -> execute -> dispose. The app must not edit platform C++ to add a package. QNX is the default/primary target; Marathon uses the same consumer API and separately built compatible native source.

Reuse [QNX Nitro runtime](../../packages/qnx-nitro-runtime/), [constrained generator](../../scripts/nitro-platform-codegen.mjs), [Expo bootstrap](../../packages/expo-modules-core-qnx/), [Expo autolinking](../../packages/expo-qnx-autolinking/), [v1 policy](../platform-modernization/NITRO_V1_API_POLICY.md), and preparation [PR79](https://github.com/mikevocalz/react-native-qnx/pull/79). Cross-reference existing workstreams 00/06/10/12. Generation of CMake is not runtime registration; the existing PlatformInfo generator is not general upstream QNX/Marathon Nitrogen support.

Keep Fabric/Hermes/JSI/Qt, existing owned queues and framework-internal TurboModule contracts. New application-facing native APIs use the actual audited Nitro v1 contract. Do not rewrite Worklets/RNGH/Reanimated as a Nitro facade. Their real bootstrap remains Phase3 work.

## Roster and ownership

All human owners/reviewers are unassigned until accepted; no vendor participation or completed subagent work is implied.

- API architect: typed public specs/call sites in `packages/qnx-nitro-runtime/specs/` and proposed generic fixture package; reviewer: independent API/lifetime engineer. Deliverable: units, ready resources, errors and cleanup before bindings.
- Nitro/codegen engineer: `scripts/nitro-platform-codegen.mjs`, generated adapters and runtime registration; reviewer: C++/JSI engineer. Deliverable: reproducible supported type shapes, negative generation tests and exact ABI alignment.
- Expo compatibility engineer: `packages/expo-modules-core-qnx/`, `packages/expo-qnx-autolinking/`, `packages/expo-qnx-config-plugins/`, integrated `packages/expo-qnx/`; reviewer: Expo/runtime engineer. Deliverable: explicit platform discovery, linkage and actual Expo module lookup/behavior.
- Native platform engineer: `native/platform/qnx/`, `native/platform/linux-host/`, affected `cmake/` and runtime bootstrap; reviewer: QNX/Qt owner. Deliverable: target-specific implementation and install/dispose ordering without duplicate dispatchers.
- Package/security engineer: manifests, exports, dependency reports, generated ownership; reviewer: independent supply-chain reviewer. Deliverable: no missing package content, target collisions or unauthorized payloads.
- Integration QA engineer: proposed `tests/integration/native-packages/` and existing JS/C++/device tests; reviewer: separate target QA owner. Deliverable: package lifecycle on external apps and named targets, not just isolated probes.

## Mandatory skills and API-first workflow

Read repository [native skill](../../.agents/skills/qnx-native-api/SKILL.md), [validation skill](../../.agents/skills/qnx-platform-validation/SKILL.md) and [skill/source policy](../platform-modernization/SKILLS.md).

1. Margelo `api-design`, source `1e9e17be6f41c838db472d6a4b943fc26355a3be`, `skills/api-design/SKILL.md`: define two success/failure/cleanup call flows; keep availability separate from a ready object's guaranteed contract.
2. Margelo `build-nitro-modules`, same source, `skills/build-nitro-modules/SKILL.md`: specifications and generator outputs must agree; generated files are not hand-edited; class boundaries follow resource ownership.
3. Margelo `cpp`, same source, `skills/cpp/SKILL.md`: RAII, explicit owner contexts, no worker-to-GUI blocking wait, no JS callback under a mutex or after runtime destruction.
4. Expo `expo-module`, source `a9e5bd3fa00b4e61f632e43f85da73aee1942582`, `plugins/expo/skills/expo-module/SKILL.md` and module-config reference: explicit support/discovery is separate from a Swift/Kotlin/native port.
5. Callstack library/performance/CI skills at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`: packaged generated outputs, meaningful runtime tests and exact-run evidence.

Read/installation/decision/test status must be recorded separately in each implementation PR. This planning author read source guidance; no installation or native execution is claimed. The inspected Nitro baseline is core/adapter/Nitrogen `1.0.0-alpha.nightly.105.g69d365fc9b18`; use v1 class declarations, HybridClass registration and constructor lookup. Generic v0 examples do not override it. Any upgrade must explicitly update all matched artifacts and retest. Expo Modules API2 refers to API compatibility, not blindly installing an npm major version2. Do not fabricate platform decorators or claim upstream generator support.

## Proposed public contract and fixture

First choose one small non-premium resource fixture with a cached typed observation, asynchronous operation, additive observation subscription and idempotent disposal. Its exact v1 declaration must be accepted by the selected generator before it becomes a public API.

Caller flow A: discover capability, create a ready resource, perform an asynchronous operation, observe completion, remove the subscription and dispose. Caller flow B: unsupported/denied capability rejects creation; a timeout or cancellation has explicit semantics; no successful resource or fake result appears. Caller flow C: two resources coexist, one is disposed/reloaded, and the other continues without stale callbacks crossing instances.

Cheap cached observations remain synchronous. Blocking file/IPC/compute work crosses once to an owned native context. Cancellation, caller timeout, server-side-effect cancellation and shutdown are different contracts. Preserve accepted-work accounting during shutdown. Hooks are optional adapters above the same imperative API; no hidden error-swallowing wrappers.

## Implementation slices

1. Inventory supported, unsupported and unknown native dependency declarations for each target; distinguish JS-only packages and intentionally optional dependencies. Report required unsupported/unknown native packages before build instead of silently omitting them. Do not infer every JS package is native.
2. Audit generator/runtime types: values, errors, optional/union shapes, arrays, callbacks, Promise values/void, returned resources, constructors, inheritance and buffer accounting. Extend one demonstrated shape at a time with deterministic generation and malformed-spec tests.
3. Generate native target inclusion and registration from declared packages. Test static-link retention, duplicate names/targets, absent installer and installation order in the same Hermes runtime. No shared QNX/Linux binary reuse.
4. Execute the fixture through Nitro and a separately declared Expo module through the supported Expo lookup path. Do not let one successful path stand in for the other. Package metadata is not an implementation.
5. Prove install/import/build/run/update/remove in the external Phase1 app, including cold rebuild after package removal, missing-registration failures and no stale generated entries. Add the same public API receipt on Marathon as its target host becomes available.

## Gates and evidence

Run the repository's frozen dependency, typecheck, JS, architecture and codegen checks. Add real compiled/linked API tests, runtime error controls and reload/disposal stress with relevant sanitizers. Attribute host, QNX cross-build, QNX physical and Marathon results separately. A handwritten isolated experiment cannot close generic generation acceptance.

- [ ] Supported package installs without app-specific platform patches.
- [ ] Generated signatures, registration and runtime/core/generator ABI agree.
- [ ] Real success, failure, async observation and disposal execute in the app.
- [ ] Missing installer, unsupported target, incompatible ABI and duplicate registration fail clearly.
- [ ] Cancellation/reload/shutdown and two-consumer tests pass without late callbacks.
- [ ] Update/uninstall remove stale code and retain documented app data behavior.
- [ ] Each target receives its own runtime evidence; absent target evidence remains blocked/not-run.

## Security, licensing, performance and rollback

Native package code and build scripts remain trusted dependencies requiring source review; validation is not an OS sandbox. Reject malformed generated paths/tokens, preserve native authorization, and exclude SDK secrets, evaluation payloads and Forge contents from core artifacts. Generic package hooks must work with Forge absent. Optional licensed integration is separately authorized and tested in its consumer.

Measure binding overhead, allocation and copies before performance claims; do not add async overhead to cheap reads or claim zero-copy without tracing the actual boundary. No target/runtime work is executed in this planning PR. Rollback disables the experimental package path and restores matching specs, generated source, runtime versions and persisted-format compatibility. Existing PlatformInfo/dispatcher proof stays available; do not erase it because broader support is unfinished.
