# QNX and Marathon delivery program

This program turns the MarathonOS and QNX Neutrino review into independently reviewable delivery plans. It preserves React Native Fabric, Hermes, Yoga and the existing Qt Quick backend. The PR series contains engineering plans and two repository skills; it does not implement or certify the future features described in their acceptance criteria.

Start with the [roster](ROSTER.md), [skills and native workflow](SKILLS.md), and [machine-readable catalog](catalog.json). Each workstream specifies architecture boundaries, paths, proposed DRI/reviewer roles, prerequisites, public contracts where relevant, implementation slices, failure tests, performance/security limits and rollback. Human implementation assignments are unassigned. Documentation PRs are independent; implementation dependencies still have to be satisfied.

## What the source review changes

| Source lesson | Application to this platform | Delivery plan |
| --- | --- | --- |
| Marathon treats gestures as interruptible native state with measured release behavior | Finish cancellation, real Worklets/RNGH/Reanimated seams and end-to-end gesture proof | 02 |
| Its frame work measures presentation and repeated frames, and avoids unnecessary hidden work | Trace input through JS/Fabric/Qt to presentation; measure before changing scheduling | 03, 05 |
| Its keyboard and lifecycle contracts cross a real application boundary | Implement composition, focus, keyboard geometry, visibility and background capability policy | 04, 05, 13 |
| Typed native services separate UI from Linux platform integrations | Design common domain APIs with separate QNX and Marathon adapters and native authorization | 06, 07 |
| QNX IPC and process isolation can preserve UI responsiveness and service recovery | Put blocking calls on owned workers, validate protocol/lifetime, reconnect with a new generation | 07, 08 |
| Shared memory can reduce measured bulk-data copy cost | Introduce bounded buffer ownership only after profiling identifies the need | 09 |
| Marathon profiles distinguish hardware capability from shell behavior | Derive viewport, scale, input and fold/hinge behavior from actual providers | 11 |
| A native Wayland window is only one part of an app platform | Design app identity, navigation, lifecycle, permission and managed-runner packaging separately | 13 |
| Existing QNX runtime and Forge experiments already provide useful foundations | Reuse their code and exact evidence instead of creating competing hosts/queues | 01, 10 |
| A shared substantial app exposes interactions that unit probes miss | Run the same NYC-Tron/Forge-oriented conformance workload on both targets | 12 |

## Existing implementation PRs

The main branch inspected for this program is `e84b51430c239bc89dbd48ac35afab89f7c77c01`. The package manifest, rather than historical README text, identifies the reviewed graph: RN `0.88.1`, React `19.3.0`, Expo `58.0.22`, Metro `0.87.1`, Nitro core/adapter/Nitrogen `1.0.0-alpha.nightly.105.g69d365fc9b18`.

| Existing PR | Inspected head | Evidence to reuse | Scope still open |
| --- | --- | --- | --- |
| [#44 — Pi 400 runtime and Nitro](https://github.com/mikevocalz/react-native-qnx/pull/44) | `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d` | Physical RN/Fabric/Hermes/Qt Quick, fullscreen Hello World, owned Qt/Nitro completion queue and teardown tests | Reliable graphics cold boot; measured performance; full module/lifecycle support |
| [#46 — Pointer lifecycle](https://github.com/mikevocalz/react-native-qnx/pull/46) | `b2c1edafd70ccc4d3bb1a6545b29d1d30ee771c2` | Opt-in cancellation/lifecycle implementation and successful ON build | New runtime cases, full OFF validation, device gesture behavior |
| [#45 — Animation integration audit](https://github.com/mikevocalz/react-native-qnx/pull/45) | `4cd69d69374ff6708528bde173f6b39ab5858017` | Source-backed Worklets/Reanimated/RNGH map and compatibility blockers | QNX runtime hooks, gesture adapter and animated property behavior |
| [#42 — Shared Rust QNX host](https://github.com/mikevocalz/react-native-qnx/pull/42) | `5f6df2cf5ae49b3466d90eec8f5641a020bd3303` | Seven-engine cross-build and physical native proof, with functional Tally coverage | JS binding, Nitro alpha alignment, other-engine functionality and app interaction |

These heads are a dated review snapshot. Re-read the PR and lockfile before implementation. Historical logs do not automatically validate a new revision. #44's JS shares the Qt owner thread; its queue is an existing prerequisite. The platform-info codegen adapter in main is constrained and is not general upstream QNX Nitrogen support.

## Workstream catalog

The PR URLs and dependency records are maintained in `catalog.json`. The initial review version may contain unassigned PR URLs while branches are being opened; the completed publication update fills them from returned GitHub results.

| Plan | Deliverable | Draft PR |
| --- | --- | --- |
| 00 | Program, native skills and ownership roster | Publication pending |
| 01 | Runtime baseline, startup recovery and thread ownership | Publication pending |
| 02 | Input lifecycle, Worklets, Reanimated and Gesture Handler | Publication pending |
| 03 | Frame pacing, input latency and performance traces | Publication pending |
| 04 | Text composition, keyboard, focus and accessibility | Publication pending |
| 05 | Visibility, background policy and state restoration | Publication pending |
| 06 | Typed platform services, capabilities and permissions | Publication pending |
| 07 | Versioned QNX IPC, deadlines and event ownership | Publication pending |
| 08 | Service isolation, supervision and recovery | Publication pending |
| 09 | Measured shared-memory and media buffer ownership | Publication pending |
| 10 | Forge JS bindings, Nitro ABI and native lifetime | Publication pending |
| 11 | Device metrics, adaptive layouts and foldable profiles | Publication pending |
| 12 | Shared NYC-Tron and Forge conformance application | Publication pending |
| 13 | Marathon native client and managed runner contract | Publication pending |

## Execution order

1. Review program boundaries and staff the roles; reconcile the runtime baseline from #44. Preserve the working recovery boot.
2. Start input, measurement, text, lifecycle, typed services, Forge binding design and device metrics as separate slices. Implementation changes to shared runtime files must be sequenced by the integrator.
3. Build QNX IPC behind the typed service contract, then recovery. Evaluate shared buffers only when the measurement plan shows a material copy-path cost.
4. Run the substantial conformance app as capabilities become available. Promote a target only after its own physical and failure-path evidence is attached.
5. In the Marathon fork, review the ordinary native-client contract first and the managed runner/schema/security contract separately. Upstream acceptance is required before claiming official managed-app support.

Plans describe feature-level dependencies, not a requirement to merge all documentation before beginning a small authorized implementation. They also do not require duplicating existing #42/#44/#45/#46 work. Keep native experiments opt-in and default OFF until their target gate is met.

## The supplied Neutrino reference

The [Himmele archive at the user's exact commit](https://github.com/Himmele/My-Blog-Repository/tree/b1fd3e9410c1a6a0d7217c3be7e324e900470d2a/QNX%20Neutrino%20RTOS) is a conceptual reference for channels, messages, resource managers, timers, threads, synchronization and shared memory. It is not a QNX 8 portability layer to vendor wholesale.

The reviewed examples contain old API and architecture assumptions. In particular, the timer example demonstrates a receive timeout, not an end-to-end client request deadline; old receive-ID types, interrupt entry points, pointer casts, permissive resource-manager examples and legacy graphics need a current SDP audit. Plans 07–09 use the official QNX 8 migration, IPC, security and memory contracts to turn those concepts into bounded native service behavior. None of these PRs copies the archive's sample code or PDFs.

## Primary source pins

- [React Native QNX main](https://github.com/mikevocalz/react-native-qnx/tree/e84b51430c239bc89dbd48ac35afab89f7c77c01), plus the exact prerequisite PR heads above.
- [Marathon Shell](https://github.com/patrickjquinn/Marathon-Shell/tree/c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5): gesture tracker, compositor frame scheduling, lifecycle, text input, device overlays, permission service, runner and app manifest schema. The companion RFC targets the owner's fork at the same inspected base.
- [QNX 8 migration](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.qnxsdp.migration/topic/kernel.html), [MsgSend](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/m/msgsend.html), [TimerTimeout](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.lib_ref/topic/t/timertimeout.html), and [resource-manager security](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.security.system/topic/manual/write_resource_manager.html).
- [Qt Quick scene graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html) and [RN threading model](https://reactnative.dev/architecture/threading-model), checked again against selected installed source before a threading change.

## Evidence and completion policy

Read [VALIDATION.md](VALIDATION.md) for checks actually performed on this documentation/skill series. Native, target and performance gates remain pending for the implementation plans. Record pass/fail/not-run/blocked/historical separately. Keep every draft PR honest about its limits; a checklist is not an execution log.

Every implementation report must include source inspection, changed paths, skill/source/decision table, exact commands and artifacts, QNX regression, Marathon-specific result, security and module licensing impact, measured budget or explicit unmeasured status, independent reviewer, next slice and rollback. QNX's RTOS architecture does not establish hard-real-time behavior or safety certification for JavaScript/React Native.

No new workflow, dependency upgrade, device deployment, boot edit or renderer change is made by this program PR. Reverting these planning and skill files removes the program guidance without changing runtime behavior.
