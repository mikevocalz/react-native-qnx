# BLOCKERS.md

## PR 161 current native and consumer candidate (2026-11-14)

Local software blockers are corrected: nested child removal and nested-root
bounds have failing/passing controls, the GCC convolution fixture is
deterministic, and the main merge preserves transactional QtSvg staging.
The standalone host graph passes 8/8; all 17 staging fixtures pass.

All nine hosted workflows now pass at `13a0e49`, including both renderer lanes
and all four corpus shards at the original limits. The earlier timeout cause
remains unestablished. The standalone consumer packaging omission is corrected
with the exact reviewed PR 159 port; original-source failure control and a
fresh dual consumer install plus both Metro bundles pass. The packaging
follow-up still requires its own published-head hosted checks.

A fresh combined candidate build with licensed QNX SDP 8.0.5 and the
selected public Qt closure, followed by the actual target SVG cases, is also
required. This Cloud environment has neither that SDP nor a connected target.
An operator with access to the existing build host and authorized target must
capture the evidence. Keep this native PR draft until that requirement is
met; prior Pi 400 receipts remain scoped to their original source and recipe.
[Evidence and limits](../validation/pr161-nested-roots-2026-11-14.md).

## PR 161 cross-build recipe correction — 2026-11-13

The native consumer used inherited Release/O1 flags with selected GCC warning
exceptions; public Qt libraries were O2. Its launcher reused upstream objects
without independently checking flags/header closure, while project sources
were recompiled. Device behavior and hashes stand as scoped evidence. Use a
fresh build without that launcher for final combined qualification.
[Corrected receipt](../validation/pr161-device-gallery-2026-11-13.md).

## PR 161 gallery and target continuation — 2026-11-13

Required checks passed at a52ffff. The current-source Pi run then exposed the
SvgCss root import; it now uses the pinned CSS subpackage and has a failing/
passing Metro regression. The twelve-card gallery renders on the real lean
VM/public QtSvg runtime, including bundled/SVG/HTTPS images and live
ForeignObject input blocking. Circle/group counters and rotation pause pass.
The added nested-invalidation control fails without outward forwarding; all
eight restored native host CTests pass. Prior review threads are resolved.
This follow-up still needs its exact-head checks and independent review.
[Artifacts, element table and rollback](../validation/pr161-device-gallery-2026-11-13.md).

## PR 161 corpus continuation — 2026-11-13

The new corpus runner needs exact-head Debug CI. Per-element device checks from the November 13 handoff remain open; a local standalone renderer pass does not close them.
[Receipt](../validation/pr161-corpus-shards-2026-11-13.md).

## PR 105 review state — 2026-11-10

The mandatory handoff omission is addressed by this follow-up. Hosted checks
passed at implementation `c09a5b1`; publication and review of the follow-up's
own checks remain the next step. No physical QNX/Marathon or general Nitrogen
support is asserted. These documentation changes do not close any v1 release
gate. See [the exact evidence](../validation/pr105-merge-readiness-2026-11-10.md).

## GHSA-vfj7-8cjw-p6xm — No official braces fixed release

As of 2026-10-03, npm latest is 3.0.3 and upstream PR #72 is still open.
The checked-in depth-guard backport mitigates the tested string/AST depth paths.
The guarded audit verifies every installed implementation before accepting this
one finding as locally mitigated; it retains other moderate+ failures. Raw
registry audit remains nonzero. Replace the backport when an official release
is available, following `docs/security/braces.md`. Physical QNX blockers below
are unaffected.

## Bundled React probe readiness (2026-10-04)

The shared probe's positive and negative runners pass against the current RN 0.88 /
Hermes / Qt 6.11.1 graph in companion PR #28 (`24ba958`). Those host changes are
required; main alone is not a passing runtime baseline. The combined CMake build
and all 11 CTests passed, including the two probe tests and 45 host-runtime cases.
Source and Metro checks are not substitutes for that run. Licensed SDP and Pi
execution remain required afterward. See [probe instructions](../react-render-probe.md).

## QNX-RN-008 — Current physical QNX AArch64 validation pending

- **First observed:** 2026-07-19
- **Updated:** 2026-10-02
- **Affected milestone:** Current physical-target exit gate
- **Current target baseline:** QNX SDP 8.0.5 with QSTI 0.5.0 build 50 on
  Raspberry Pi 4 or Raspberry Pi 5.
- **Ready now:** The repository has a same-day physical-target lane:
  `scripts/pi-preflight.sh` validates the host toolchain, SSH, AArch64le and
  `/dev/screen`; `scripts/install-pi.sh` configures/builds, deploys
  atomically, runs three clean Screen cycles by default, captures stdout/slog2
  and a screenshot when available, then verifies and hashes the evidence.
- **Historical proof:** QNX SDP 8.0.2 on Windows x86-64 successfully
  cross-built the AArch64 C++20/Screen targets. A QNX x86_64 VM also ran the
  C++20 probe and `rnqnx_playground`; the Screen probe initialized a 1280x720
  surface, rendered 600 frames, and exited cleanly.
- **Important boundary:** That SDP 8.0.2 evidence predates the Expo SDK 58 /
  React Native 0.88 migration. It remains historical evidence and is not a
  substitute for current SDP 8.0.5/QSTI validation.
- **Remaining work:** Run the current lane on physical Pi hardware and capture
  the generated evidence directory. Current SDK-58 QNX cross-compilation should
  also be rerun on the licensed 8.0.5 host.
- **Commands:**

  ```bash
  export QNX_SDP_ENV=/path/to/qnxsdp-env.sh
  pnpm pi:preflight
  pnpm pi:install
  ```

- **Exit criteria:** AArch64le target preflight passes; the Screen app starts and
  exits cleanly; stdout/slog2 are captured; visual evidence is captured when the
  QNX screenshot utility is present; the exact QSTI/SDP identity is recorded.
- **Scope:** Blocks calling the current physical Pi/QNX target verified. It does
  not block host development or the already-merged SDK-58 tooling line.

## Milestone 1 — Current Hermes/Qt target runtime not proven

- **Updated:** 2026-10-02
- **Current source identity:** React Native 0.88.0-rc.3 resolves
  `hermes-compiler` 260318099.0.4 and records bundled Hermes source
  `hermes-v260318099.0.2`.
- **Resolved software work:** The current Hermes QNX source rebase is merged.
  Exact-source preparation, the current QNX patcher, host-`hermesc`
  cross-configuration, and the CMake import layer are in-tree and their
  source-contract CI is green.
- **Remaining external proof:** Cross-build Hermes/JSI with licensed QNX SDP
  8.0.5, execute `rnqnx_hermes_qnx_probe` on AArch64le QSTI, then move the
  host-proven React/Fabric/Qt Quick runtime onto the target.
- **Scope:** Does not block the native Screen/Pi lane. It blocks a truthful
  current-line React Native/Hermes/Qt runtime-on-QNX claim.

## RESOLVED — UPSTREAM-DEP-001 dependency audit blocker

- **First observed:** 2026-08-13
- **Resolved:** 2026-10-01
- **Historical problem:** The old Expo 57 / Metro graph reported high-severity
  `image-size` development-tool advisories with no compatible published fix at
  the time.
- **Current status:** The Expo SDK 58 graph no longer reports that blocker.
  The fresh audit instead exposed patchable transitive xmldom, js-yaml and
  brace-expansion findings; the repository now scopes patched overrides and
  validates their resolved versions.
- **Proof:** The permanent `SDK 58 validation` workflow passes
  `pnpm audit:dependencies` at the configured moderate threshold on `main`.
- **Scope:** No active dependency-audit blocker remains from UPSTREAM-DEP-001.

## Active blocker summary

1. Physical QSTI Pi 4/5 evidence on the current QNX SDP 8.0.5 line.
2. Licensed SDP 8.0.5 cross-build/runtime proof for the current Hermes, Nitro,
   and Expo QNX seams.
3. Full RN + Hermes + Fabric + Qt Quick target runtime proof.

There is no current npm dependency-audit blocker.

## PR #111 — remaining qualification (2026-11-10)

The CI omission is fixed at `295f7240099c7371dc8988f4d411b1840888a415`:
new Qt v2 feature tests can no longer be silently absent from an enabled lane.
The replacement hosted ON/OFF renderer results remain pending. Inspect their
actual CTest inventory and results on that commit before accepting the CI gate.

QNX cross-build/device execution and an actual Marathon compositor run are
still missing for the v2 slice. They require the selected licensed QNX SDP and
matching Qt closure, an authorized target operator and independent target
review. Prior PR #110 interaction is evidence for its separate candidate, not
this branch. No additional owner permission is needed for the already-authorized
software validation; unavailable target evidence keeps this PR draft.
