# QNX and Marathon delivery program

This program turns the MarathonOS and QNX Neutrino review into independently reviewable delivery plans. It preserves React Native Fabric, Hermes, Yoga and the existing Qt Quick backend. This follow-up supplies the expanded program, API/source requirements and updated repository skills; it does not implement or certify the future features described in their acceptance criteria.

The owner requires [Nitro v1 API-first design](NITRO_V1_API_POLICY.md). The [Neutrino coverage map](NEUTRINO_REFERENCE_MAP.md) assigns all 19 archive categories. Published Plan15 covers the broader official QNX documentation/example audit; the source map below records the three official pages checked in this readiness review.

**Publication baseline:** this expansion follows merged [program PR47](https://github.com/mikevocalz/react-native-qnx/pull/47) and is prepared against main `48e37f7550b4244f8a7936c01af75419e1fec416`. The current premium-product boundary and PR46 validation receipts are preserved.

Start with the [roster](ROSTER.md), [skills and native workflow](SKILLS.md), and [machine-readable catalog](catalog.json). The roster and catalog identify proposed ownership and dependencies. Each published delivery plan proposes architecture boundaries, exact paths, prerequisites, public contracts where relevant, implementation slices, failure tests, performance/security limits and rollback. Human implementation assignments are unassigned. Documentation PRs are independent; implementation dependencies still have to be satisfied.

## What the source review changes

| Source lesson                                                                                | Application to this platform                                                                                          | Delivery plan |
| -------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- | ------------- |
| Marathon treats gestures as interruptible native state with measured release behavior        | Finish cancellation, real Worklets/RNGH/Reanimated seams and end-to-end gesture proof                                 | 02            |
| Its frame work measures presentation and repeated frames, and avoids unnecessary hidden work | Trace input through JS/Fabric/Qt to presentation; measure before changing scheduling                                  | 03, 05        |
| Its keyboard and lifecycle contracts cross a real application boundary                       | Implement composition, focus, keyboard geometry, visibility and background capability policy                          | 04, 05, 13    |
| Typed native services separate UI from Linux platform integrations                           | Design common domain APIs with separate QNX and Marathon adapters and native authorization                            | 06, 07        |
| QNX IPC and process isolation can preserve UI responsiveness and service recovery            | Put blocking calls on owned workers, validate protocol/lifetime, reconnect with a new generation                      | 07, 08        |
| Shared memory can reduce measured bulk-data copy cost                                        | Introduce bounded buffer ownership only after profiling identifies the need                                           | 09            |
| Marathon profiles distinguish hardware capability from shell behavior                        | Derive viewport, scale, input and fold/hinge behavior from actual providers                                           | 11            |
| A native Wayland window is only one part of an app platform                                  | Design app identity, navigation, lifecycle, permission and managed-runner packaging separately                        | 13            |
| Existing QNX runtime and optional Forge experiments already provide useful foundations       | Reuse their code and exact evidence instead of creating competing hosts/queues                                        | 01, 10        |
| A shared substantial app exposes interactions that unit probes miss                          | Run the same non-premium conformance workload on both targets; test licensed Forge packages separately and optionally | 12            |

## Existing implementation PRs

The main branch inspected for this program is `e84b51430c239bc89dbd48ac35afab89f7c77c01`. The package manifest, rather than historical README text, identifies the reviewed graph: RN `0.88.1`, React `19.3.0`, Expo `58.0.22`, Metro `0.87.1`, Nitro core/adapter/Nitrogen `1.0.0-alpha.nightly.105.g69d365fc9b18`.

| Existing PR                                                                                 | Inspected head                             | Evidence to reuse                                                                                              | Scope still open                                                                               |
| ------------------------------------------------------------------------------------------- | ------------------------------------------ | -------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------- |
| [#44 — Pi 400 runtime and Nitro](https://github.com/mikevocalz/react-native-qnx/pull/44)    | `e9026fac8ce8deb5a8e6ba4c35d1dcc5592d478d` | Physical RN/Fabric/Hermes/Qt Quick, fullscreen Hello World, owned Qt/Nitro completion queue and teardown tests | Reliable graphics cold boot; measured performance; full module/lifecycle support               |
| [#46 — Pointer lifecycle](https://github.com/mikevocalz/react-native-qnx/pull/46)           | `e783e2f000f20ef849b96021387860beaa7b4e34` | Merged input fixes, host14/14 ON and13/13 OFF, synthetic Qt input on physical Pi400                            | USB/touch hardware, React component interaction, clock origin, Wayland and gesture recognition |
| [#45 — Animation integration audit](https://github.com/mikevocalz/react-native-qnx/pull/45) | `4cd69d69374ff6708528bde173f6b39ab5858017` | Source-backed Worklets/Reanimated/RNGH map and compatibility blockers                                          | QNX runtime hooks, gesture adapter and animated property behavior                              |
| [#42 — Shared Rust QNX host](https://github.com/mikevocalz/react-native-qnx/pull/42)        | `5f6df2cf5ae49b3466d90eec8f5641a020bd3303` | Seven-engine cross-build and physical native proof, with functional Tally coverage                             | JS binding, Nitro alpha alignment, other-engine functionality and app interaction              |

[PR48](https://github.com/mikevocalz/react-native-qnx/pull/48) subsequently proved12 raw Fabric JavaScript callbacks under synthetic Qt injection on physical QNX, with the interception control failing at the intended JS assertion (host15/15 ON,13/13 OFF). This does not prove hardware input or React component interaction.

PRs42,44,45 and46 are merged; the later program/publication base is `48e37f7550b4244f8a7936c01af75419e1fec416`. The original source snapshot above is retained as provenance. These heads are a dated review snapshot. Re-read the PR and lockfile before implementation. Historical logs do not automatically validate a new revision. #44's JS shares the Qt owner thread; its queue is an existing prerequisite. The platform-info codegen adapter in main is constrained and is not general upstream QNX Nitrogen support.

## Workstream catalog

The PR URLs and dependency records are maintained in `catalog.json`. The predecessor program was merged as PR47 and this follow-up is published as [PR49](https://github.com/mikevocalz/react-native-qnx/pull/49). Workstreams01–12 and14–15 are now published as PRs50–63, with returned URLs and inspected heads in the catalog. They propose future behavior; their implementation gates remain open. Workstream13 in the Marathon repository is still unpublished, with a proposed destination and null PR/commit.

| Plan | Deliverable                                                                      | Publication                                                   |
| ---- | -------------------------------------------------------------------------------- | ------------------------------------------------------------- |
| 00   | Nitro v1 API contract, complete QNX source map, skills and delivery roster       | [#49](https://github.com/mikevocalz/react-native-qnx/pull/49) |
| 01   | Runtime baseline, startup recovery and thread ownership                          | [#50](https://github.com/mikevocalz/react-native-qnx/pull/50) |
| 02   | Input lifecycle, Worklets, Reanimated and Gesture Handler                        | [#51](https://github.com/mikevocalz/react-native-qnx/pull/51) |
| 03   | Frame pacing, input latency and performance traces                               | [#52](https://github.com/mikevocalz/react-native-qnx/pull/52) |
| 04   | Text composition, keyboard, focus and accessibility                              | [#53](https://github.com/mikevocalz/react-native-qnx/pull/53) |
| 05   | Visibility, background policy and state restoration                              | [#54](https://github.com/mikevocalz/react-native-qnx/pull/54) |
| 06   | Typed platform services, capabilities and permissions                            | [#55](https://github.com/mikevocalz/react-native-qnx/pull/55) |
| 07   | Versioned QNX IPC, deadlines and event ownership                                 | [#56](https://github.com/mikevocalz/react-native-qnx/pull/56) |
| 08   | Service isolation, supervision and recovery                                      | [#57](https://github.com/mikevocalz/react-native-qnx/pull/57) |
| 09   | Measured shared-memory and media buffer ownership                                | [#58](https://github.com/mikevocalz/react-native-qnx/pull/58) |
| 10   | Generic package hooks and optional licensed Forge compatibility                  | [#59](https://github.com/mikevocalz/react-native-qnx/pull/59) |
| 11   | Device metrics, adaptive layouts and foldable profiles                           | [#60](https://github.com/mikevocalz/react-native-qnx/pull/60) |
| 12   | Non-premium platform conformance application with optional licensed integrations | [#61](https://github.com/mikevocalz/react-native-qnx/pull/61) |
| 13   | Marathon native client and managed runner contract                               | Proposed; unpublished                                         |
| 14   | QNX 8 systems probes and legacy sample modernization                             | [#62](https://github.com/mikevocalz/react-native-qnx/pull/62) |
| 15   | Current QNX documentation, examples and release audit                            | [#63](https://github.com/mikevocalz/react-native-qnx/pull/63) |

## Premium product boundary

Nitro Forge is a separate premium product, not a platform component, default dependency or required conformance workload. Generic Nitro/package/lifecycle hooks belong in the platform; Forge production bindings and package configuration belong with Forge or its licensed consuming app. Core builds, CI and releases use non-premium fixtures. Plan10 is optional commercial compatibility work and is not a prerequisite for core plan12 acceptance. Preserve the existing opt-in external-checkout experiment as historical evidence without vendoring or redistributing premium content.

## Execution order

1. Review program boundaries and staff the roles; reconcile the runtime baseline from #44. Preserve the working recovery boot.
2. Start input, measurement, text, lifecycle, typed services, generic package integration and device metrics as separate slices. Implementation changes to shared runtime files must be sequenced by the integrator.
3. Build QNX IPC behind the typed service contract, then recovery. Evaluate shared buffers only when the measurement plan shows a material copy-path cost.
4. Run the non-premium conformance app as capabilities become available; licensed package integration is a separate opt-in consumer check. Promote a target only after its own physical and failure-path evidence is attached.
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

No new workflow, dependency upgrade, device deployment, boot edit or renderer change is made by this documentation/skill series. Reverting these planning and skill files removes the program guidance without changing runtime behavior.
