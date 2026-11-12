# Plan 10 — Generic Nitro v1 package integration and optional licensed Forge interoperability

## Publication baseline and Nitro v1 gate

The publication baseline is main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`, including merged PRs #42, #44, #45, #46, #47 and #48. The current [owner policy](https://github.com/mikevocalz/react-native-qnx/blob/48e37f7550b4244f8a7936c01af75419e1fec416/AGENTS.md) was inspected at that revision; native source links retain the earlier inspected implementation pins. Earlier source pins identify historical evidence; no runtime result is promoted to a new revision without revalidation.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Product boundary and deliverable

This is a documentation-only plan for **generic package discovery, linking, Nitro v1 registration and lifecycle hooks** in React Native QNX and its Marathon target. Prove those hooks with a non-premium package fixture. The intended consuming-app workflow is **install a selected licensed package → import → build/run**, using documented tooling without platform source patches or per-component manual wiring. This remains an acceptance goal, not shipped installation support.

Nitro Forge is a separately sold premium product. Its production adapters, generated bindings and package configuration belong with Forge or the consuming app. Forge must not become a platform/system component, default dependency, bundled sample requirement, or prerequisite for platform build, boot, CI or release. Removing Forge must leave the platform usable. No premium engines, assets, binaries or entitlement code enter platform distributions.

A separate, explicitly authorized licensed-consumer lane can prove one Tally document opened, edited, undone, snapshotted and reopened with deterministic disposal before Hermes teardown. Expand that lane only after each engine passes its own target tests. Neither its success nor its availability gates the core platform.

## Exact technical evidence

[PR #42](https://github.com/mikevocalz/react-native-qnx/pull/42), head `5f6df2cf5ae49b3466d90eec8f5641a020bd3303`, cross-builds the seven-engine archive and proves Tally behavior, queue ordering, native lifetime and entropy/thread probes on a physical Pi 400. Its default-OFF external-checkout adapter is a compatibility experiment: preserve its probes and evidence without treating it as a production packaging architecture or redistribution permission. It exposes internal C++ ownership, not a JavaScript binding. Seven archive members are not seven functionally validated editors.

[PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44), head `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d`, already proves the owned Qt/Nitro completion route, typed/void Promises, checkpoints and close races on the Pi. Reuse it. Its [dispatcher contract](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) states that JS shares the Qt GUI owner and close drains accepted tasks; close is not cancellation or a deadline. Producer shutdown remains binding work.

The inspected optional Forge baseline is `2f33f2fb204077c4a493788ee36890c76f03b31a`. Its [Factory spec](https://github.com/mikevocalz/nitro-forge/blob/2f33f2fb204077c4a493788ee36890c76f03b31a/packages/ignition-v1-lab/src/specs/ForgeFactory.nitro.ts) and [Document spec](https://github.com/mikevocalz/nitro-forge/blob/2f33f2fb204077c4a493788ee36890c76f03b31a/packages/ignition-v1-lab/src/specs/ForgeDocument.nitro.ts) use v1 classes with Apple/Android annotations. QNX pins alpha105; that Forge binding baseline uses alpha120. No ABI compatibility is assumed.

## Resolve integration contracts first

**Generic package integration:** audit the selected React Native/Nitro package metadata and target tooling before specifying an extension schema. Discover only installed, selected packages; generate reproducible linkage/registration from their declared metadata. Diagnose unsupported targets, missing artifacts, duplicate class names and ABI mismatches before use. Define package startup and producer-disposal hooks without Forge names or engine knowledge. Package-owned exports must resolve the actual v1 registered class and constructor; a successful import alone is insufficient evidence. Do not invent a QNX decorator or present a new manifest as existing upstream support.

**Generated/runtime ABI:** use an exact matched Nitro/Nitrogen/core graph for the non-premium fixture. For the optional licensed lane, either regenerate/adapt Forge against the pinned alpha136 runtime (device-proven on alpha 105 only), or propose a separate runtime migration with complete revalidation of #44's patch, dispatcher and teardown contracts. Record source/archive hashes, full lockfile and compiler/include compatibility. Do not silently upgrade to the latest nightly or make Forge's revision a core requirement. Main's narrow `nitro-platform-codegen.mjs` generates platform-info bindings; that does not prove general asynchronous document/session generation or upstream QNX Nitrogen support.

**Execution ownership:** Forge's [HybridForgeDocument implementation](https://github.com/mikevocalz/nitro-forge/blob/2f33f2fb204077c4a493788ee36890c76f03b31a/packages/ignition-v1-lab/cpp/HybridForgeDocument.cpp) uses `ThreadPool::shared()` through `ForgeSerialQueue`. #42's QNX owner already owns a native worker and its lifecycle. For the optional licensed adapter, select one document owner and reuse/refactor the proven ownership contract in Forge or the consuming app. Do not stack a serial queue around that owner and block a Nitro worker on `future.get()` while duplicating admission and teardown policy.

Within the licensed product adapter, preserve the Rust `nf-sys` ABI, existing engines, `ForgeRustSession`, queue semantics and NFD1 format. Keep Fabric/Hermes/Yoga/Qt Quick for rendering. Package integration introduces no second renderer or Skia/egui/WebView component backend.

## Optional licensed API contract and caller examples

Preserve the existing Factory/Document surface: asynchronous `open`, `apply`, `snapshot`, `restore`, `undo`, `redo` and idempotent `close`, plus cheap observed `kind`. Generated headers from the chosen exact v1 must define the actual C++ signatures; generic skill snippets using older Nitro shapes are not authoritative.

The [current contract](https://github.com/mikevocalz/nitro-forge/blob/2f33f2fb204077c4a493788ee36890c76f03b31a/packages/ignition-v1-lab/src/contracts.ts) has a typed outer `ForgeCommand` but serialized `payloadJson`, `dataJson` and event details. Preserve and document that bounded transitional representation; do not describe it as fully typed engine payloads or verified nested-AnyJSON codegen. Validate commands and payload limits before resource-intensive work. A later engine-specific API can improve that contract without inventing support in this slice.

These examples belong to the separately authorized consuming-app fixture, using the installed licensed package's exported v1 class after real registration exists. The package resolves its actual class through `NitroModules.getHybridClass` and construction; users must not hand-register each engine. These are proposed workflows, not executable QNX evidence today. Exact exports and error reporters must be supplied by that fixture.

```ts
const factory = new ForgeFactory();
const document = await factory.open('tally');
try {
  await document.apply({
    op: 'setCell',
    payloadJson: JSON.stringify({ addr: 'A1', value: '42' }),
  });
  await document.undo();
  await document.redo();
} catch (error) {
  reportError(error); // Preserve a real rejected Error; keep the app usable.
} finally {
  await document.close();
}
```

```ts
const source = await factory.open('tally');
let bytes: ArrayBuffer;
try {
  bytes = await source.snapshot();
} finally {
  await source.close();
}
const restored = await factory.open('tally');
try {
  await restored.restore(bytes);
  // The fixture also mutates/corrupts a separate buffer and expects rejection.
} finally {
  await restored.close();
}
```

`restore` must retain an owning copy or proven owner before returning to JS; temporary JSI memory cannot be borrowed by the worker. `snapshot` returns owned bytes, not an unproven Rust/GPU zero-copy promise. UI preview is ephemeral animated state; a completed edit commits one command and updates the application store. Cancelling that preview commits nothing; submitted native work needs separately defined cancellation semantics. File persistence requires Plan 06's permissioned storage path; an in-memory snapshot does not authorize arbitrary filesystem access.

## Implementation sequence and lifetime contract

1. Audit metadata, versions and generation. Add a proposed `RNQNX_ENABLE_NITRO_PACKAGE_INTEGRATION` experiment, default OFF, with a non-premium package fixture. Reject incomplete configurations. Preserve #42's existing opt-in compatibility adapter; add no platform-owned production Forge binding or per-engine switch.
2. Prove generic discovery/linking, v1 class construction, async results, owned bytes, failure and cleanup through that fixture. Copy/own inputs, bound work, settle exactly once and keep recoverable failures from poisoning later operations. Cached external-memory reporting must not synchronously query a worker on the JS owner.
3. Integrate generic producer disposal into host shutdown: close work admission, drain accepted native work, allow required completion settlement, release runtime-affine captures, then close the completion queue and destroy Hermes. Specify reentrancy. Never join a worker synchronously waiting for the GUI/JS owner.
4. Define abandonment and failure behavior. Dropped JS references are not cancellation; final disposal releases sessions on their owner. A shutdown deadline needs its own producer protocol and failure policy; queue drain is not that deadline. Use an external watchdog for hangs.
5. In Forge/the authorized app, align ABI and implement the optional factory/document adapter against one owner. Exercise Tally through a release bundle and Qt/Fabric view, including atomic invalid-command rejection, undo/redo, restore and separate-process reopen.
6. Validate package update and removal: rebuild generated registrations/artifacts, reject stale ABI combinations and prove core startup/tests still work with no Forge installation, credentials or cached artifacts. Successful consumer packaging must require no platform patches or manual component registration.

The generic slice depends on #44 and Plan 01; #42 is optional compatibility evidence for the licensed lane. Plans 02/03/05/06/12 supply input, measurement, lifecycle, storage and non-premium conformance. Plan 13's Marathon packaging needs an independent Linux build; it cannot consume QNX binaries.

## Roster and skills

These are proposed implementation responsibilities; no human assignment or external maintainer approval is asserted.

| Role                      | Concrete ownership                                                                                                                     | Independent reviewer        |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------- | --------------------------- |
| Native API/Nitro DRI      | Generic hooks in `packages/qnx-nitro-runtime/`; audited generator integration; proposed `tests/native/package-integration/`            | Nitro v1/codegen reviewer   |
| Licensed product engineer | Forge-owned spec/binding/package paths, starting from audited `packages/ignition-v1-lab/`; preserve `nf-sys`, queue ownership and NFD1 | Native ownership reviewer   |
| Qt/Hermes engineer        | Generic host producer-disposal seam and existing dispatcher                                                                            | Runtime lifetime reviewer   |
| Dependency/build engineer | Generic discovery/linking, exact pins/lockfiles and target guards; retain `cmake/RnqnxForge.cmake` as compatibility-only               | QNX toolchain owner         |
| Consumer/QA engineer      | Non-premium core fixture; separately owned, authorized licensed-app Tally fixture                                                      | Independent device reviewer |

Sources were read in the required order and applied to this plan, not executed as a native build:

| Skill / exact revision                                                                                                                                                                                                            | Decision changed                                                                                                              |
| --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| [Margelo api-design, `1e9e17be6f41c838db472d6a4b943fc26355a3be`](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md)                                         | Preserve ready document handles, actual errors and cleanup; show caller workflows before implementation.                      |
| [Margelo build-nitro-modules, same revision](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md)                                                    | Require matching generated/runtime bindings and native-state ownership; do not invent a QNX decorator or broad codegen claim. |
| [Margelo cpp, same revision](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md)                                                                                    | One owner route, RAII resources, copied buffers and no worker-to-JS blocking cycle.                                           |
| [Callstack react-native-best-practices, `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) | Measure edit-to-visible behavior and memory while engine work stays off the shared GUI/JS thread.                             |

The skills are guidance; actual pinned v1 source wins over generic examples. Read the upgrade/CI skills at recorded revisions if those implementation scopes change. CLI installation, generated QNX documents and device results are not claimed here.

## Separate acceptance lanes

**Core, non-premium:** require installation/import/build from a clean consumer, correct registration and native behavior, typed/void Promises, Error propagation, input ownership and cleanup. Test missing metadata/artifacts, unsupported targets, duplicate classes, ABI mismatch, updates, stale build output and uninstall. Core CI/release passes without Forge sources, packages, credentials or artifacts. Re-run #44's dispatcher regression with the integrated graph.

**Optional licensed consumer:** additionally require ready documents, FIFO/atomic edits, undo/redo, owned snapshots, separate-process reopen and correct rendering. Mutate/release input after `restore` submission. Test duplicate `close`, post-close calls, failed open, malformed/oversized payloads, two documents, dropped references and callbacks during unmount/reload. Record license authorization and this lane's result separately; unavailable commercial coverage does not block a core release.

Proposed lifetime gate per applicable fixture: 100 open/work/close cycles and 30 shutdown/admission-race rounds under an external watchdog. Account for every accepted operation; silently abandoned Promises fail. Report memory trends after warmup and p50/p95/p99 operation-to-visible latency under Plan 03. Performance gains remain unclaimed until measured.

Existing core commands include `pnpm install --frozen-lockfile`, `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `pnpm nitro:platform:check` and the host renderer suite. The platform-info check cannot validate general package or Forge generation. #42's `ctest --test-dir build/forge-host --output-on-failure` remains a separately opted-in external-checkout experiment, not core CI. Implementation must supply actual package-generation and physical-app commands; those are not yet defined.

This planning PR runs no native/application tests. Historical results retain their exact PR heads. QNX runtime changes require the selected SDP/Qt build and JS/Fabric device run; the optional Forge lane additionally needs its licensed Rust/product closure. Marathon needs its own Linux/Wayland and sandboxed-app run. Report hosted CI separately. Keep each implementation draft until its relevant gates pass.

## Security, release and rollback

Use bounded inputs, trusted package artifacts and permissioned storage; never log document content or expose native addresses. Preserve the licensed adapter's reviewed Rust entropy path and archive provenance. Inspect platform package/release contents: exclude external premium source/build outputs, engines, assets, binaries and entitlement code. Only an expressly licensed consuming-app artifact may include permitted product content. Retain notices and SDK/Qt distribution restrictions; do not create entitlement implementations in RNQNX.

Documentation rollback removes this plan. Runtime rollback disables the generic experiment and restores matching packages/generated code/bundles together. Removing optional Forge regenerates consumer linkage while leaving core build, boot, tests and release usable; retain compatibility probes and recovery boot. No NFD1 migration is planned. Next implement the generic metadata/ABI contract with a non-premium fixture; licensed Tally interoperability follows in its owning product/app.
