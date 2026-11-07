# Plan 02 — Native input, Worklets, Reanimated and gesture integration

## Publication baseline and Nitro v1 gate

This branch is based on main `1baad40a98362ce151ea73bd6f0a6e1d3020926e`. PRs #42, #44, #45, #46 and #48, plus program #47, are merged at this publication snapshot. Earlier source pins below identify inspected evidence rather than unmerged work. No runtime result is promoted to a new revision without revalidation. The current owner policy keeps premium Forge packages optional and outside core build, conformance and release requirements.

Application-facing native work must follow the owner's [Nitro v1 API-first contract](https://github.com/mikevocalz/react-native-qnx/blob/f06bc134d71fe410207525e0348b2fa2481c369b/docs/platform-modernization/NITRO_V1_API_POLICY.md): actual v1 class/registration/constructor APIs, aligned core/runtime/generator versions, typed operations, explicit errors and cleanup, and owned asynchronous work. API sketches below describe semantics; they are not older-version binding implementations or proof of supported code generation.

## Deliverable and present status

This PR adds an implementation plan. It does not install animation packages, enable a feature flag, implement a recognizer, or establish QNX/Marathon runtime support. Deliver one interruptible drawer and one scroll interaction through the existing Fabric/Hermes/Yoga/Qt Quick renderer, then expand supported gestures from measured evidence.

The starting point is more specific than “add gestures.” Merged [PR #46](https://github.com/mikevocalz/react-native-qnx/pull/46), final inspected head `e783e2f000f20ef849b96021387860beaa7b4e34`, implements opt-in terminal/cancel handling in `QtTouchDispatcher`. Its final receipts record host **14/14 ON and 13/13 OFF** and a QNX cross-build, plus deterministic QObject-filter input injection on a physical Pi400. The target probe consumes production Fabric payloads in a native listener. This establishes neither physical USB/touch delivery nor target JS-handler delivery; host Fabric-to-Hermes tests are separate. Global coordinates, real pressure, touch-to-mouse deduplication, transformed clipping and clock equivalence remain open. It does not implement RNGH.

The follow-up [PR #48](https://github.com/mikevocalz/react-native-qnx/pull/48), inspected at `173605c413fb19b6adcf682e9ec1293be9093c4d` and merged as `1baad40a98362ce151ea73bd6f0a6e1d3020926e`, now proves deterministic synthetic Qt input through Fabric EventBeat/runtime scheduling into a raw JavaScript handler on physical Pi400. Its receipt records 12 ordered callbacks with native payload parity, a same-artifact interception control that fails at the JS count, host 15/15 ON and 13/13 OFF, and a QNX cross-build. This closes the raw target-JS delivery gap for the injected fixture. It does not prove physical USB/touch, React Pressable/responder interaction, clock/coordinate fidelity or any animation/gesture library.

[PR #45's exact-source audit](https://github.com/mikevocalz/react-native-qnx/blob/4cd69d69374ff6708528bde173f6b39ab5858017/docs/reanimated-gesture-handler-qnx-audit.md) identifies Reanimated 4.7.1, Worklets 0.13.1 and RNGH 3.5.0 as investigation targets. Expo 58.0.22 selects a different Worklets patch and RNGH line; the audit records an RN compatibility conflict. These are candidate versions, not an approved installed graph. Main `e84b51430c239bc89dbd48ac35afab89f7c77c01` and current manifests take precedence over older README version summaries.

## Marathon lessons and architectural boundary

At Marathon `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`, [GestureTracker](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/shell/src/services/gesturetracker.cpp) estimates recent velocity, rejects stale velocity and starts settling from the release state. Its [scrolling investigation](https://github.com/patrickjquinn/Marathon-Shell/blob/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5/docs/perf/SCROLLING.md) tests catch-and-reverse behavior and accidental tap activation. Those become behavioral fixtures here. RNGH and Reanimated must still execute their own compatible contracts; copying a QML spring would not port either library.

Use one native pointer ownership policy across Qt Flickable, React responders and RNGH. Define recognition, simultaneous recognition, failure dependencies, capture, cancellation and target removal before enabling competing handlers. A press that arrests a fling must not also activate the underlying row. Preserve real event times, coordinate spaces and device capabilities; the current dispatcher still assigns screen coordinates from page coordinates and uses constant force. Synthetic mouse events generated from touch must not create a second logical stream.

Worklets needs a distinct Hermes UI runtime, UI scheduler, frame source, value-transfer implementation and RN CallInvoker. [PR #44's dispatcher](https://github.com/mikevocalz/react-native-qnx/blob/e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d/docs/nitro/QT_DISPATCHER.md) already supplies owned completion delivery into the RN runtime; reuse that boundary. It does not supply a Worklets runtime. Initially both runtime owners may be the Qt GUI thread, because that is the present host topology. Separate runtimes do not imply parallel threads or protection from a long RN JS task. Plan 01 owns any later JS-thread separation decision. Qt view mutation remains on its GUI owner; Yoga remains the layout authority.

Marathon's shell/client resampling distinction also matters: resample at most once in the owner of an input stream, only after measuring the actual panel cadence. Do not apply shell smoothing, client smoothing and a new RN filter independently.

## Implementation slices and dependencies

1. **Extend the merged pointer evidence.** Start from merged #46/#48/#44, preserve the host ON/OFF, injected-target raw-JS and interception-control receipts, then prove physical USB/touch-to-React responder delivery and source-backed coordinate/synthesis behavior. Rerun ON/OFF for the actual changed candidate; do not recreate already merged work. Keep `RNQNX_ENABLE_INPUT_LIFECYCLE` OFF until its target gate passes. Preserve a regression case for legacy OFF behavior.
2. **Resolve compatibility and bootstrap.** Record the exact package/archive/commit graph and Expo departure. Amend ADR-0004 through review for the real third-party Worklets/Reanimated/RNGH bootstrap contracts: the existing RN-core registry exception does not automatically authorize them. No empty module providers or invented Nitro facade. Regenerate the lockfile with any accepted version change.
3. **Prove Worklets alone.** Add a proposed, default-OFF `RNQNX_ENABLE_WORKLETS` target with platform guards. Implement ownership and initialize/invalidate order against the audited interfaces. Lean Hermes requires a release bytecode path: prove all six unpackers, captured values, RN/UI round trips and teardown. Detect Babel source fallback and fail the build. The existing isolated 256-byte HBC transformation proves only one transformation.
4. **Prove one Reanimated property path.** Add the real platform holder, frame timestamps, Fabric hooks and supported property application. Begin with opacity and translation, with explicit rejection for unimplemented required behavior. Do not spoof Apple/Android macros or redefine Yoga geometry to make compilation succeed.
5. **Prove one RNGH tap/pan path.** Implement registry/native components, tag lifetime, state transitions and arbitration. Add pinch/rotation only after the first path's cancellation and multi-contact identity are correct. Audit RNGH 3 consumer hook usage against its pinned release rather than copying a version-2 snippet.
6. **Connect the shared fixture.** Keep drag preview in animated values; use Zustand for application/document state and commit one non-premium fixture operation after the completed interaction. An authorized optional Forge consumer may run the same interaction contract separately. Cancellation commits nothing. Plans 03 and 12 own performance and end-to-end receipts; Plans 05 and 13 supply lifecycle and Marathon-shell integration.

Plan 01 is the runtime prerequisite; #45, #46 and #48 are existing work, not duplicates to replace. Planning review can proceed independently. Runtime support cannot be promoted from a documentation merge.

## Ownership and required skills

Responsibilities are proposed; human DRIs and independent reviewers remain unassigned until an implementation PR names them.

| Role                         | Owned paths / deliverable                                                                                            | Independent review                         |
| ---------------------------- | -------------------------------------------------------------------------------------------------------------------- | ------------------------------------------ |
| Qt/Fabric input DRI          | `vnext/src/fabric/QtTouchDispatcher.cpp`, matching header, `QtScrollBridge.cpp`, `tests/cpp/QtReactHostHostTest.cpp` | RN event-semantics reviewer                |
| Worklets runtime engineer    | `vnext/src/runtime/`, proposed focused scheduler/runtime files, CMake platform target                                | Hermes/JSI lifetime reviewer               |
| Dependency/build engineer    | `package.json`, `pnpm-lock.yaml`, Metro/Babel configuration, source receipts                                         | QNX regression owner                       |
| Gesture integration engineer | Proposed native recognizer adapter and shared interaction fixture                                                    | Qt input owner plus accessibility reviewer |
| Device QA/performance owner  | QNX and Marathon receipts, Plan 03 trace correlation                                                                 | Reviewer who did not implement the port    |

All four source skills below were read for this plan; this records guidance applied to design, not CLI installation or executed native tests.

| Skill and pinned source                                                                                                                                                               | Decision applied                                                                                                                 |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| [Margelo api-design](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/api-design/SKILL.md)                                         | Preserve public library semantics and explicit unsupported behavior; introduce no workaround API.                                |
| [Margelo build-nitro-modules](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/build-nitro-modules/SKILL.md)                       | Reuse real runtime/binding ownership; do not describe an unrelated dispatcher or hand-written shim as generated library support. |
| [Margelo cpp](https://github.com/margelo/react-native-skills/blob/1e9e17be6f41c838db472d6a4b943fc26355a3be/skills/cpp/SKILL.md)                                                       | Separate scheduler, recognizer and conversion files; retain runtime-affine captures until owned teardown.                        |
| [Callstack react-native-best-practices](https://github.com/callstackincubator/agent-skills/blob/59e9d17446a248b21d86f078fcd0bd2d36db1ca4/skills/react-native-best-practices/SKILL.md) | Measure the actual gesture and JS/native work; do not infer smoothness from compilation or component counts.                     |

No new public Nitro API is specified here. If implementation needs one, require its exact v1 generated shape and two caller examples before native code. The generic skill examples do not override audited v1 interfaces. Software Mansion animation/gesture/multithreading guidance must be re-read at an exact available source before claiming it was applied to implementation.

## Acceptance and falsification

| Gate               | Required evidence                                                                                                                                                                                               |
| ------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Input correctness  | Exactly one terminal event per accepted contact; no stale responder after cancel, grab loss, deactivation or unmount; correct terminal coordinates/timestamps; mixed mouse/touch does not duplicate activation. |
| Runtime isolation  | All six bytecode unpackers execute on lean Hermes; closures transfer correctly; cross-runtime references never outlive their owner; source fallback causes a reproducible build failure.                        |
| Library behavior   | Real package imports bootstrap; pan updates the real Reanimated value; interrupted settling starts from current position; cancel does not commit document changes; nested scrolling has one documented winner.  |
| Destruction        | Repeated surface mount/unmount and runtime reload with queued work produces neither callback-after-destruction nor an unsettled accepted Promise. Exercise admission races without sleeps.                      |
| Device interaction | At least 20 recorded repetitions each of fling-stop, reverse-drag, nested scroll and multi-contact cancel on physical QNX and separately on Marathon; retain failed trials.                                     |
| Performance        | Use Plan 03's panel-specific budgets and presentation measurements. Publish p50/p95/p99 and repeated frames; no 60/120-fps or real-time guarantee from host tests.                                              |

Existing repository commands to run during implementation, **not executed by this planning PR**, include `pnpm install --frozen-lockfile`, `pnpm validate:dependencies`, `pnpm typecheck`, `pnpm test:js`, `pnpm validate:architecture`, `pnpm codegen:check`, and the `linux-host-renderer` CMake/CTest presets. Run full renderer ON/OFF matrices with the actual new options and real release Metro/HBC fixtures; record exact commands after configuration exists. A no-tests configuration is failure, not a pass.

QNX acceptance requires a licensed SDP build and a physical run of the combined changes. Marathon requires a separate Linux/Wayland build, compositor/window identity and touch run. #44/#46/#48 provide revision-specific runtime, native-event and raw-JS injected-input proof; they cannot establish physical-device gesture, React responder or Marathon support. This PR performs source/design review only; application installs, compiler, sanitizer and device results are pending.

## Security, rollout and rollback

Keep input owned by the focused/authorized surface; test destroyed surfaces and display transitions without injecting into another client. Do not add root access or raw global input capture. Audited animation packages declare MIT; preserve notices if source is later adapted, and review Qt/QNX distribution separately. Performance flags must not raise RT priority without Plan 01/03 evidence.

Ship each experiment OFF, enable only in a conformance build, then review per-platform support rows. Rollback of this PR is removal of this document. Runtime rollback disables the new animation/gesture options and restores the prior bundle/lockfile together; disabling #46 alone must not leave dependent adapters partially active. The next concrete implementation slice is the physical input-to-React responder and coordinate/synthesis gate on merged #46/#48, followed by the explicit version/bootstrap decision and isolated Worklets proof.
