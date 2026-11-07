# Phase 5: MarathonOS as an additional native target

Status: proposed implementation plan; documentation only.
Planning base: main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, 2026-11-07.
QNX remains primary/default; retain the name React Native QNX and repository/package identities.

## Outcome and dependencies

Build the same external Expo application for MarathonOS without copying its screens, routes, state or business logic. Reuse shared Hermes/JSI/Fabric/Yoga/Qt Quick source and a separately compiled Linux/Wayland host. This is not a browser version, Android-emulated app, second renderer or replacement of Marathon's Qt/QML shell.

Begin upstream/fork source, ABI, packaging and lifecycle contract audits during Phase0/1. Development-client work consumes [Phase1](https://github.com/mikevocalz/react-native-qnx/pull/82) app identity and [Phase2](https://github.com/mikevocalz/react-native-qnx/pull/83) relevant package contracts. Managed acceptance integrates [Phase3](https://github.com/mikevocalz/react-native-qnx/pull/84) navigation/input and [Phase4](https://github.com/mikevocalz/react-native-qnx/pull/85) service/lifecycle providers. This is a dependency on relevant interfaces, not a reason to postpone all Marathon investigation.

QNX builds and releases must not require Marathon tools, compositor services or upstream approval. Marathon receives an independently justified support label. Existing iOS/Android/web targets, when present in the consumer, retain their own behavior.

## Sources to reuse, not duplicate

Read [Marathon architecture](../marathon/ARCHITECTURE.md), [native host](../marathon/NATIVE_HOST.md), [roster](../marathon/ROSTER.md), [skills](../marathon/SKILLS.md), the [Marathon Metro resolver](../../packages/metro-config-marathon/src/index.js) and workstream13 in the [existing program](../platform-modernization/README.md).

The owner's [Marathon-Shell PR1](https://github.com/mikevocalz/Marathon-Shell/pull/1) already contains the managed-client RFC at `d16e6db81f5bafde099c5452995f8f46bd05a51c`. Its inspected upstream baseline is `c2c1b2fe32aeaa51edf75a45045cbfbbc08c4ac5`; this is historical source provenance, not a claim that it is still the latest shell. Re-audit actual manifest, runner, security, lifecycle and Wayland interfaces before implementing. Extend that work through focused companion implementation PRs rather than opening another duplicate RFC.

## Two distinct native milestones

Development client: compile the existing Marathon host against the target's architecture, libc, Qt and C++ ABI; launch a normal xdg-shell window inside actual Marathon with the app's reverse-DNS identity. Verify its Metro platform identity and shared app bundle. A desktop glibc binary is not automatically an Alpine/musl target binary. QNX SDK/Screen libraries must not leak into the Marathon link graph.

Managed application: implement the reviewed native executable delivery route across schema, installer, launcher, identity and confinement together. The existing RFC's QML-only entry assumptions cannot be bypassed with a dummy QML file secretly executing an arbitrary binary. Decide the supported native-entry or approved alternative packaging route against current source and record fork-only versus upstream-supported status honestly.

Do not link React Native/Nitro into the compositor merely to expose a lifecycle protocol. The client owns its runtime. Generic native-client interfaces should be reusable by other toolkits. No warm runtime pooling until cross-app native/JS state, permission and secret isolation has separate proof.

## Roster and exact ownership boundaries

All human owners/reviewers are unassigned until accepted; maintainer names are not implied endorsements.

- Linux/Wayland client engineer: `native/platform/linux-host/`, `cmake/`, existing Marathon host target; reviewer: Qt/toolchain engineer. Deliverable: target-compatible native binary, app_id and actual compositor connection.
- Metro/Expo engineer: `packages/metro-config-marathon/`, `packages/react-native-marathon/`, shared consumer CLI/config; reviewer: QNX resolver owner. Deliverable: distinct marathon identity and unchanged QNX behavior.
- Client lifecycle/API engineer: runtime adapters and generated Phase2 service contracts; reviewer: JSI/lifetime engineer. Deliverable: semantic Back, readiness, activation, freeze/thaw and teardown behavior.
- Shell runner engineer: companion fork `tools/marathon-app-runner/` and audited lifecycle paths; reviewer: shell/security engineer. Deliverable: versioned handshake without privileged or blocking JS calls in the shell.
- Packaging/security engineer: fork schema, installer/launcher and sandbox paths identified by fresh audit; reviewer: independent security/installer reviewer. Deliverable: signed-content coverage, identity, confined paths and update/rollback/remove semantics.
- Device QA engineer: client/fork integration tests and actual Marathon target receipts; reviewer: separate platform owner. Deliverable: launch/interaction/lifecycle and failure-path evidence with existing QML-app regressions.
- QNX regression owner: QNX builds/bundles/device lane; reviewer: platform integrator. Deliverable: no Marathon dependency or QNX regression from shared changes.

The platform owns portable client/runtime code. Shell-native contracts belong in the Marathon fork. Forge-specific bindings remain with the licensed product/consumer, not either platform distribution.

## Skills and source decisions

Apply repository `qnx-native-api` and `qnx-platform-validation`; follow Margelo `api-design` -> `build-nitro-modules` -> `cpp` at `1e9e17be6f41c838db472d6a4b943fc26355a3be`, under `skills/<name>/SKILL.md`, for client-facing native APIs. Decisions: explicit ready resources/errors, actual supported Nitro v1 codegen/registration and owner-thread disposal. The shell's generic protocol does not require a Nitro dependency.

Callstack `react-native-best-practices`, `github-actions` and `create-react-native-library` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` guide measured client behavior, target artifacts and package boundaries. Expo `expo-module` at `a9e5bd3fa00b4e61f632e43f85da73aee1942582` guides explicit native support. Discover/pin any additional Linux/Qt/security/packaging skills before claiming their use. Official Qt QPA/Wayland and current shell source are mandatory audits, not invented agent skills.

Planning source guidance is read; installation/native execution is not claimed. Record exact source commit/path, installation/read status, applied decision and test result per implementation. Actual v1 policy overrides older generic examples. Never use mobile-only automation as Marathon device evidence.

## Bounded implementation sequence

1. Freeze actual shell/toolchain/manifest versions and the development-versus-managed delivery decision. Test proposed schemas against existing applications before changes.
2. Build/run the external Phase1 app in Marathon as a development client. Verify no QNX headers/libs, correct Wayland identity/scale and real input; label unconfined development launch accurately.
3. Add versioned lifecycle/readiness and semantic Back/activation. Test timeout, disconnect, late reply, root fallback, duplicate activation and an app continuously repainting. Never treat repaint as acknowledgment.
4. Implement managed identity, permission mediation, storage ownership and executable path/argv validation through the chosen reviewed route. Verify wrong identity and denied/revoked capability failures at the native boundary.
5. Implement install/update/rollback/uninstall, preserving declared user-data policy and existing QML applications. Add freeze/resume, app crash and service restart tests before declaring the same app integrated.

## Acceptance and evidence

- [ ] Same application source, route tree and public APIs; only target configuration/adapters differ.
- [ ] Separate Linux target binary with compatible architecture/libc/Qt and no QNX dependency.
- [ ] Real Marathon compositor launch, app identity, scale, keyboard/touch and route behavior.
- [ ] Semantic Back/activation/freeze-thaw produce no duplicate actions or stale-instance callbacks.
- [ ] Managed installation enforces signature/content/path/identity and permission contracts.
- [ ] Update/rollback/removal preserve documented data semantics; existing QML apps regressions pass.
- [ ] Repeated launch/close and lifecycle races show no cross-app or stale-resource leakage.
- [ ] QNX remains independently buildable/testable/releasable without Marathon or Forge.

Use distinct Linux offscreen, generic Wayland, actual Marathon shell, VM and physical receipts. Attach exact source/toolchain, bundle/binary hashes, flags, logs/screenshots/video and negative-control results. Run current fork-prescribed tests after source inspection; do not invent a universal shell build command. Performance uses real first-content/presentation/input/RSS/idle observations, not assumed 60/120fps.

## Limits, security and rollback

No official upstream, store, phone or managed-runtime support is claimed by this document. Native code needs actual confinement and authorization; JS checks are insufficient. No root, unrestricted D-Bus, sandbox-disable or forged identity to make a demo pass. SDK/runtime/source terms and Qt/plugin notices are reviewed separately; premium products stay unbundled.

Rollback implementation by disabling the new native-entry mode and reverting client/shell contracts together while retaining old manifest readers, normal app launch and user data. Preserve QNX recovery boot. This planning PR changes no shell/runtime source or release gate and runs no target tests.
