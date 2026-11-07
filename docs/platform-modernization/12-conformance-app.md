# Plan 12: one shared application for platform conformance

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Status and intended outcome

This document defines a proposed implementation and evidence lane. It adds no application code and reports no new test run. Build one substantial React Native application that exposes the behavior users need from QNX and Marathon: a virtualized document list, an interruptible navigation drawer, multilingual text editing, a non-premium native document fixture, a permissioned system subscription and state recovery.

Use the same application source and assets on both targets, with audited platform adapters below it. Keep NYC-Tron component/style reuse subject to exact source, licensing and non-premium dependency checks. A visually similar mock, a native-only probe and a screenshot of Hello World are useful evidence for their respective layers; this application must exercise the actual user-visible integration.

Nitro Forge is a separately sold premium product. Core CI, conformance, samples, builds and releases must succeed without installing, licensing or accessing it. RNQNX owns generic runtime support and Nitro v1 discovery, linkage and lifecycle; it must not acquire a built-in Forge dependency. Do not vendor or redistribute premium source, engines, assets or binaries in this application or its artifacts. Optional premium compatibility belongs to a separately authorized, licensed consuming app outside the core fixture.

## Baseline and source inspection

The repository baseline is `e84b51430c239bc89dbd48ac35afab89f7c77c01`. Its [component support matrix](https://github.com/mikevocalz/react-native-qnx/blob/e84b51430c239bc89dbd48ac35afab89f7c77c01/docs/component-support.md) identifies specific gaps despite some green symbols: software-keyboard avoidance is a no-op, accessibility properties do not reach a screen reader, and modal lifecycle events are incomplete. Record behavior, not the symbol count, when selecting scenarios.

The [playground](https://github.com/mikevocalz/react-native-qnx/tree/e84b51430c239bc89dbd48ac35afab89f7c77c01/apps/rn-playground) is a real bundle fixture; `apps/component-conformance` and `apps/performance-lab` are scaffold directories in this base. Reuse the former as the proposed application location, and keep measurement helpers shared instead of growing two competing applications.

Relevant existing work must remain correctly labeled:

| Existing work | Inspected revision | Evidence boundary |
| --- | --- | --- |
| [PR #44](https://github.com/mikevocalz/react-native-qnx/pull/44) | `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d` | Physical Pi 400 runtime and owned Nitro completion queue; no application-level frame budget |
| [PR #42](https://github.com/mikevocalz/react-native-qnx/pull/42) | `5f6df2cf5ae49b3466d90eec8f5641a020bd3303` | Historical default-OFF, external-checkout Forge experiment; compiling seven native engines does not prove JS/UI behavior or create a core conformance prerequisite |
| [PR #45](https://github.com/mikevocalz/react-native-qnx/pull/45) | `4cd69d69374ff6708528bde173f6b39ab5858017` | Reanimated/Worklets/Gesture Handler source audit, not a QNX runtime integration |
| [PR #46](https://github.com/mikevocalz/react-native-qnx/pull/46) | `e783e2f000f20ef849b96021387860beaa7b4e34` | Merged, default-OFF pointer lifecycle: host14/14 ON,13/13 OFF and injected Qt events on physical QNX; physical USB/touch and target JS-handler delivery remain unproven |

[PR #48](https://github.com/mikevocalz/react-native-qnx/pull/48), head `173605c413fb19b6adcf682e9ec1293be9093c4d`, subsequently adds physical Pi400 synthetic input-to-raw-Fabric-JS proof (12 ordered callbacks and a failing interception control), with host15/15 ON and13/13 OFF. Physical USB/touch and React component/responder interaction still require their own application scenarios.

Marathon's [native lifecycle contract](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/NATIVE_APP_LIFECYCLE.md), [device profile guide](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/DEVICE_OVERLAYS.md) and [security model](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/SECURITY_MODEL.md), all at `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`, motivate lifecycle, adaptive-input and permission-denial scenarios. Source behavior is not evidence that RN already integrates it.

## Application design and scenarios

Keep Fabric, Yoga, Hermes and the Qt Quick backend intact. The core application consumes native APIs owned by Plans 05, 06 and 11; it introduces no alternative bridge, renderer or production storage engine. Use a small test-only Nitro v1 document module to exercise generic native discovery, linkage, typed operations and lifetime without premium dependencies. Reuse NYC-Tron components only after inspecting their actual exports and exact dependency versions. Freeze the selected non-premium source commits in the implementing PR; no compatibility claim follows from their names alone.

| Scenario | User workflow | Required observable behavior |
| --- | --- | --- |
| List and drawer | Scroll a long list, start opening the drawer, reverse it, then tap an item | Smooth interruption, correct final destination, no accidental item press after a cancelled gesture |
| Text and focus | Edit Latin, Arabic, Devanagari and emoji text; move selection; open a modal; return | Composition survives controlled updates, selection stays valid, focus returns to the correct field |
| Native document editor | Open a seeded non-premium fixture, edit, undo/redo, save, close and reopen | The expected revision and synthetic data return through the actual Nitro v1 binding; disposed handles fail predictably |
| System state | Subscribe to one implemented capability; deny/revoke access; stop/restart its provider | Meaningful unavailable/error UI, bounded reconnect, latest snapshot, no duplicate listeners |
| Lifecycle | Background with and without a permitted task; return after app recreation | Hidden visual work idles, allowed native work follows policy, document/route/scroll state restore |
| Adaptive layout | Resize, attach a keyboard, change scale, exercise a two-region fixture | Readable usable layout, correct input targets, state retained; simulated topology labeled |

Use deterministic, synthetic documents and local image assets. Record font files and fallback behavior for each script. A missing font is a visible compatibility result, not a reason to replace a multilingual case with ASCII. Keep text and document contents out of routine traces.

Keep fixture persistence bounded and deterministic; it is test infrastructure, not a replacement document product. The core application has no premium engine selector, import or bundled premium sample.

An optional Plan 10 compatibility lane may use a separately authorized licensed app and premium delivery channel. Its future acceptance requires normal install, import, build and run, followed by update and uninstall, without platform patches. Test exact package/runtime/generator alignment and the real generated API. Existing default-OFF external-checkout experiments remain historical evidence, not proof that seamless consumer installation already works. Report premium results separately; unavailable licenses or packages cannot block a core result or release.

## Execution and evidence model

Each scenario has a stable ID, deterministic seed, user actions, expected state transitions and an observation method. Permit manual device interaction when automation cannot exercise the OS; require a video plus correlated native/application logs instead of calling a script successful merely because it launched.

An evidence record contains the repository and dependency commits, bundle and executable hashes, build options, OS/backend, device model, physical versus simulated label, refresh/scale, scenario seed, command and exit status, timestamps, raw trace paths and result: `passed`, `failed`, `blocked` or `not-run`. A missing target, unsupported library or unavailable profiler produces a named blocked result. No catch-all screenshot may mark all scenarios passed.

Examples of intended runs:

1. Open the list, drag the drawer halfway, reverse while its release animation is running, then select the visible third row. Compare pointer IDs, gesture decisions, navigation state and presented frames. Fail if an old animation wins after reversal or the underlying row receives a cancelled tap.
2. Type an IME composition into the editor, trigger an unrelated controlled React update, then background and resume. Capture composition/selection transitions with redacted lengths and a synthetic fixture. Fail if the text duplicates, disappears or restores into a stale surface generation.
3. Start a service subscription, revoke its permission, restart the provider, then unmount the component. Require a visible denied state, native enforcement and no newly selected callbacks after removal; an already selected delivery may complete safely without touching an unmounted surface. A mocked provider passes only the host fault-injection row.

## Delivery slices and dependencies

1. **Static native baseline.** Implement the list/editor layout and scenario selector under `apps/component-conformance`, bundle through the existing Metro configurations, and record actual supported components. Keep missing scenarios blocked.
2. **Interaction and input.** Add Plans 02 and 04 behavior once their bindings exist. Use Plan 03's presentation measurements. A Qt animation or JS transform fixture must not be labeled Reanimated support.
3. **Native data and system behavior.** Implement the non-premium document fixture and consume Plans 06–08's service contracts. Match Nitro runtime/core/generator versions before importing a HybridClass. Keep optional Plan 10 consuming-app compatibility outside the core application and required pipeline.
4. **Lifecycle and devices.** Add Plans 05 and 11 scenarios, then Plan 13's managed Marathon launch/permission/install tests. Preserve separate native-development and managed-package evidence.
5. **Permanent comparison lane.** Store immutable receipts and publish a support matrix by scenario and target. Add CI only after reading the applicable CI skill and checking actual runner/toolchain availability.

Use proposed `RNQNX_BUILD_CONFORMANCE_APP=OFF` for native target registration and a separate opt-in application bundle. Keep the existing playground and its QNX regression bundle stable. Documentation review is independent; a full runtime acceptance run depends on the implemented workstreams above and Plan 01's reproducible baseline.

## Roster and skills

These are proposed responsibilities; human assignments and approvals remain **unassigned**.

| Role | Concrete path ownership | Independent reviewer |
| --- | --- | --- |
| Application/NYC-Tron engineer | `apps/component-conformance`, fixture assets and component imports | RN/Fabric engineer |
| Native fixture engineer | Proposed `apps/component-conformance/native-fixtures`; generic Nitro v1 document and lifetime behavior | C++/JSI lifetime reviewer |
| Platform QA engineer | Proposed `tests/conformance`, scenario receipts and run driver | QNX/Marathon target owners |
| Performance engineer | Shared measurement helpers under `apps/performance-lab` or `scripts`; one implementation | Runtime/graphics reviewer |
| Accessibility and input reviewer | Multilingual, keyboard, focus and accessible-action scenarios | Product/application reviewer |

Source-read skills for this plan are Margelo [`api-design`, `build-nitro-modules`, `cpp`](https://github.com/margelo/react-native-skills/tree/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills) at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, and [Callstack `react-native-best-practices`](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4`. Applied decisions: realistic error/cleanup consumers, no handwritten substitute binding, native resource lifetime tests, and measure-before-optimize comparisons. No skill CLI install or test execution is claimed. Do not assume `agent-device` or an RN harness supports QNX/Marathon; establish its actual target capability before selecting it.

Optional premium compatibility has its own consuming-app DRI and independent native integration reviewer under Plan 10; both remain unassigned and outside this core roster's delivery prerequisites.

## Validation, budgets and failure gates

Existing recipes were inspected in the pinned `package.json`, `CMakePresets.json` and Marathon host guide. After implementation run `pnpm install --frozen-lockfile`, `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, the full `linux-host-renderer` configure/build/CTest presets, and both production Metro paths. Wire this application's actual entrypoint into checked-in build commands; no nonexistent conformance command is asserted here.

Host behavior tests must run through Hermes and the real Qt/Fabric host where that boundary matters. Offscreen pixels verify rendering only. Cross-compilation verifies ABI/build only. Separate physical QNX, Marathon QEMU, Marathon phone and keyboard-handheld rows record their actual scope. Re-run the existing QNX Screen/runtime lane independently of Marathon.

Require a clean core install/build/conformance run with no premium package, checkout, credentials or artifact access. Inspect dependency closure and release contents for accidental premium inclusion. The non-premium fixture must prove generic module discovery, missing-module failure, linkage errors, callbacks and disposal; a premium result cannot substitute for those checks.

Capture cold and warm first presentation separately, input-to-presentation p50/p95/p99, missed/repeated frames, background wakeups, native/JS memory and 100-cycle teardown retention. Plan 03 establishes target-specific budgets before results are judged; this PR supplies no invented frame rate or startup improvement. Require no accidental activation after cancellation, correct saved content, no stale-generation callback and explicit permission denial regardless of performance score.

## Security, licensing, rollout and rollback

Use only synthetic or owner-approved test data and permission requests needed for the selected scenario. Fault injection is opt-in, isolated from production service identities and documented in receipts. Audit NYC-Tron, non-premium fixtures, fonts, assets, Qt modules and redistribution rights at their exact revisions. Premium licensing and compatibility evidence stay with the separately authorized consuming app; do not copy its premium payloads into core fixtures, logs, CI caches or release artifacts. Never embed device credentials in fixtures or logs.

The next implementation PR should add the baseline list/editor application and an executed host receipt, leaving unsupported scenarios visibly blocked. Roll back by disabling the optional target and removing its launch entry; keep the existing playground functional. Preserve raw evidence for historical comparison, but do not allow historical passes to count as the result for a changed head commit.
