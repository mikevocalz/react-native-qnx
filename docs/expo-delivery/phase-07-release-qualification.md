# Phase 7: real-device qualification and React Native QNX release

Status: proposed delivery plan; documentation only. Source baseline: main `cfc28165525e0d7c54192daf58b9b4318a26e83a`, 2026-11-07.
QNX release depends on reviewed Phase0, Phase1 external app, Phase2 native packages, Phase3 interaction, Phase4 services and Phase6 developer workflow. [Phase5](https://github.com/mikevocalz/react-native-qnx/pull/86) adds the separate Marathon support qualification; unresolved Marathon upstream decisions must not indefinitely block an otherwise qualified QNX release.

## Outcome and authoritative gates

Release **React Native QNX** with QNX as primary/default, a truthful supported board/toolchain/component matrix, installable artifacts and reproducible consumer instructions. Publish Marathon's preview/beta/stable label only from its own evidence. Shared source does not share qualification automatically.

Use the existing [v1 readiness record](../releases/v1-readiness.json), [readiness guide](../releases/v1-readiness.md), [component matrix](../component-support.md), [handoff](../handoff/CURRENT_STATE.md), [packaging](../packaging.md), [Qt decision](../adr/0013-open-source-qt.md) and existing workstreams01/03/04/12/14/15. Do not create competing release booleans or mark gates complete merely because all planning PRs merged.

[evidence checker PR76](https://github.com/mikevocalz/react-native-qnx/pull/76), [performance lab PR77](https://github.com/mikevocalz/react-native-qnx/pull/77), [device tools PR78](https://github.com/mikevocalz/react-native-qnx/pull/78) and [integration PR80](https://github.com/mikevocalz/react-native-qnx/pull/80) help collect/check/reproduce evidence. They do not authenticate physical observations, provide missing instrumentation or replace independent review.

## Roster and path ownership

Human assignments are unassigned. Each gate needs an implementation/evidence owner and an independent reviewer who inspected its exact candidate receipts.

- Release captain/integrator: `docs/releases/`, candidate tags and approved packaging workflow; reviewer: QNX maintainer. Deliverable: exact candidate and explicit gate decision, no publication before authorization.
- QNX toolchain/graphics engineer: native QNX build, Qt/QPA/plugin closure and board runbooks; reviewer: independent toolchain/graphics owner. Deliverable: separately qualified public-source runtime and recovery-safe boot procedure.
- Physical-device QA: board/image-specific input/component/cold-boot receipts; reviewer: separate device/platform owner. Deliverable: repeatable observable app behavior, not synthetic-only inference.
- Performance/instrumentation engineer: existing native trace hooks, workload captures and reporting; reviewer: independent measurement engineer. Deliverable: calibrated boundary/clock/sample data and honest limitations.
- Security/licensing engineer: source notices, package inventory, signature/content/dependency audit; reviewer: independent release/security reviewer. Deliverable: allowed artifact closure, no SDK/premium/evaluation leakage.
- Fresh-developer/documentation QA: versioned guides, external consumer install/build/deploy/run; reviewer: someone outside implementation. Deliverable: literal clean-host reproduction and verified troubleshooting.
- Marathon qualification owner: actual target shell/install/lifecycle/security results; reviewer: independent Marathon platform/security owner. Deliverable: separate support disposition without obstructing QNX's valid release.

No person, vendor or upstream author is assigned merely by being named in a source. Shared runtime fixes discovered here return to the responsible phase with focused PRs and fresh candidate tests.

## Skills and source discipline

Repository `qnx-platform-validation` governs exact source, evidence separation and measurement. Read `qnx-native-api` and actual [Nitro v1 policy](../platform-modernization/NITRO_V1_API_POLICY.md) for any native fix. The required native pipeline is Margelo `api-design` -> `build-nitro-modules` -> `cpp`, source `1e9e17be6f41c838db472d6a4b943fc26355a3be`, paths `skills/<name>/SKILL.md`; apply it to fixes, not claim it was executed by collecting logs.

Callstack `react-native-best-practices`, `github-actions`, `create-react-native-library` and `writing-user-docs` at `59e9d17446a248b21d86f078fcd0bd2d36db1ca4` govern measurement, immutable artifacts, packed consumers and versioned reproducible instructions. Expo `expo-module` at `a9e5bd3fa00b4e61f632e43f85da73aee1942582` is required for module-compatibility fixes. Discover and pin applicable review/security/upgrade skills before use; neither automated reviews nor skill source reading substitute for independent release approval.

This plan records source guidance, not external skill installation, device execution or commercial/vendor endorsement. Every implementation report records source commit/path, installed version, decision changed, code paths, exact command and evidence outcome.

## Seven existing QNX gate dossiers

1. **Distribution:** identify eligible public Qt source/channel/hash, exact modules/QPA/plugins, shared-library closure, notices and applicable source/library-replacement materials. Preserve older evaluation provenance without repackaging it as open source. QNX SDP/BSP/runtime terms remain separate. The selected open-source route does not authorize every Qt component or third-party binary automatically.
2. **Candidate CI:** run all required checks on the exact candidate after integration, not only on constituent PR heads. Preserve merge-versus-head identity, feature flags, dependency lock, generated outputs and signed/hash-identified artifacts. Skipped/missing jobs are not passes.
3. **Cold boot:** record at least 20 consecutive final supervised power-cycle successes on the named release board/image with graphics/app readiness, recovery and raw logs. Preserve every failed attempt; reboots and app relaunches do not count. No incidental flashing, power cycling or boot edits by a validation script.
4. **Device parity:** give every advertised component an explicit device disposition, supported props/limits and failure behavior. Prove physical keyboard/touch and selected interaction scope on stock Pi4 QSTI. Custom Pi400 results remain separate. Additional claimed boards need their own receipts.
5. **Performance:** collect actual startup-to-first-content/presentation boundary, presented-frame intervals and RSS for Hello World, 1,000-row list and real drawer drag. Record measured refresh, samples, warmup, duration, clock alignment, instrumentation overhead and uncertainty. Queued frames, JS callback cadence and screenshots cannot substitute for presentation measurements.
6. **Developer path:** a new authorized developer starts from documented prerequisites on a clean supported host, installs versioned consumer artifacts and creates/builds/deploys/runs the external Expo app without platform source edits. Distinguish this from an empty source directory on a preprovisioned CI runner.
7. **Documentation:** publish version-matched getting-started, API/support limits, native package matrix, evidence gallery, troubleshooting, ADRs, release notes and rollback. Verify commands and links against the actual release, not a future example.

## Execution slices

Freeze one candidate and its runtime/lock/generated identities. Assemble package and license inventory first, then run required host/cross-build/native gates. Schedule supervised board tests only on authorized hardware with recovery retained. Capture input, lifecycle, fault and performance cases using the same non-premium reference app. Have independent reviewers inspect raw observations and hashes before updating gate records. Repeat relevant tests after every candidate fix; never inherit an older revision's pass automatically.

After QNX gates pass, authorize and execute a bounded release publication with checksum/provenance and rollback instructions. Marathon is evaluated separately for managed install/update/uninstall, permissions, lifecycle, actual shell interaction and supported devices. Its status may remain preview while QNX reaches v1. No store/upstream certification or hard-real-time JavaScript guarantee is inferred.

## Completion checklist

- [ ] Exact QNX candidate/board/image/toolchain and distribution inventory are approved.
- [ ] All required candidate checks pass; excluded scopes are explicit.
- [ ] Twenty consecutive supervised cold boots with recovery and all raw attempts retained.
- [ ] Physical component/input/lifecycle dispositions cover every advertised feature.
- [ ] Three real workload measurements with reviewable native boundaries and limitations.
- [ ] Independent clean-host consumer workflow reproduced without hidden checkout requirements.
- [ ] Versioned docs and authorized publication/rollback complete.
- [ ] Marathon support label justified independently; QNX release is not held by unrelated Marathon approval.

## Evidence schema, limitations and rollback

Each gate stores source/lock/binary/bundle identities, toolchain and Qt/plugin list, flags, exact commands, board/image, timestamps, observations, logs/media, hashes, author and reviewer. Use passed/failed/blocked/not-run/historical-prerequisite explicitly. Hashes establish correspondence, not truth of measurements or permission to redistribute. Raw logs omit secrets/private device addresses and licensed payloads. The checker must remain incapable of auto-promoting release acceptance from synthetic fixtures.

This planning change runs no native or physical tests, changes no readiness status and publishes nothing. Core CI/release runs without Forge; optional licensed consumer integration stays separately authorized. No destructive target action is implied by opening this plan.

Rollback a bad candidate with documented compatible package/runtime versions and validated recovery procedures, preserving app data and migration boundaries. Never erase failure evidence or silently downgrade an incompatible data schema. Reverting this planning document alone affects no runtime. A release is complete only after behavior, distribution and independent evidence review, not after PR-count completion.
