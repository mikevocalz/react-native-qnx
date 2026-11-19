# BLOCKERS.md

## PR 149 shutdown follow-up — 2026-11-14

PR 149 stays draft. Current-patch QNX SDP/device qualification is unavailable here. Historical busy-JS delivery was 0/40; dedicated React JS scheduling, remaining recognizers, layout animations and scroll handlers remain open. The local native ON result establishes static-library compilation and integrated host tests; the playground host does not link all three integration libraries, so it does not establish a Worklets application runtime. Final-head required hosted checks must pass. The full local JavaScript runner is blocked by this environment's AF_UNIX socket restriction; focused suites passed without changing that gate. Independent source reviews found no remaining blocker in the reviewed changes. A fresh CodeRabbit service review remains unavailable because its CLI agent login is unsupported here.
[Shutdown and integration receipt](../validation/pr149-shutdown-2026-11-14.md); [native ON build receipt](../validation/pr149-on-native-build-2026-11-14.md).

## PR 149 frame/mount continuation — 2026-11-13

PR 149 stays draft. Busy-JS delivery, remaining recognizers, exact-head hosted gates and independent review remain open. The combined native slice was independently reviewed; its stale documentation link is fixed. PR 161 must merge before PR 159, then PR 149.
The Hermes importer now selects the real compiler VM for opt-in Worklets;
configuration/error regressions, real-root cross-build and a lean-file-free
Pi frame probe passed. The importer review reported zero findings.
[Receipt and limits](../validation/pr149-frame-mount-2026-11-13.md).

## PR 159 review follow-up — 2026-11-14

Remaining acceptance work is exact-head required hosted CI plus current-target and release-artifact evidence. Local SourceCode and image-event host regressions are now proven by candidate/control/restoration runs. Codec attribution output, release-app animation and reproducible device-run receipts are still missing. The full local JS runner is blocked by AF_UNIX EPERM; CodeRabbit CLI remains signed out. All 11 release gates remain pending.

[Source, commands, hashes, limitations and rollback](../validation/pr159-review-followup-2026-11-14.md). Image results retain source 252b128; full-host results name b6bf7eb and a separate 55-file source inventory. Initial and restored host binaries share SHA-256 55af47f994614c2ca0110e3dd77004822b0c479772631c7afe4fbb2355712cd1. The consumer merge changes no tested source-tree identity. Prior 125-entry Release logs remain preserved with their stale-MOC omission; the corrected 126-entry run executes that disk-clear regression. Host ASan scope and uninstrumented dependencies are stated explicitly.

## PR 159 drawer route and pointer correction — 2026-11-13

The drawer now selects its navigator destination across all Home child tabs.
Native hover/focus decoration no longer obscures the row's input target.
Commit 1f809ce has failing old-source controls, passing JS/type/layout/bundle
checks, 34 QML cases, the native input regression and zero review findings.
Nine physical Pi captures show one selected row across five destinations and
all five Home tabs. The transplanted full host retains two separate failures;
exact-head CI, the clean combined build and owner-tile promotion remain open.
[Evidence, provenance and rollback](../validation/pr159-drawer-route-2026-11-13.md).

## PR 159 drawer selection correction — 2026-11-13

SVG d38097f is stacked into PR 159 with both source/evidence sets preserved.
Dependency/type/docs checks pass; the full local JS runner stops at its Linux-only
consumer-preview test on macOS. PR 161 renderer ON has two complex-corpus timeouts;
merge and clean combined target qualification remain pending.

The owner caught two teal rows after Components → Home. The renderer skipped
transparent background updates, leaving the old selection painted. Commit
cf43271 clears transparent/removed generic View fills; its regression fails
before the fix and all eight adapter cases pass with the isolated delta.
Pi captures now show one teal row for each route, including the final binary/
HBC pair. The independent review's interrupted-swipe issue is also fixed with
Zustand gesture state and three regressions. Final four-file review has zero
findings. Exact-head CI and clean combined qualification remain pending.
[Evidence and rollback](../validation/pr159-drawer-selection-2026-11-13.md).

## PR 159 drawer and module lifetime continuation — 2026-11-13

Committed d257b5a retains one TurboModule per successful host lookup; its forced
GC regression fails without retention and passes after restoration. The same
optimized device bundle now completes its image API sequence. Commit 874ea81
fixes JS drawer swipe reversal with Zustand state and native drawer dynamic
metadata/click transport. Pi captures show hover, keyboard focus and Enter
navigation; the final binary/HBC repeat navigation and image APIs. All 34
standalone QML cases pass in Release and ASan/UBSan. The transplanted Linux host
suite retains two separate failures, so only its lifetime case is qualified.
Inherited cross-build flags/object reuse are corrected in the receipts. CI found cross-test mock caching; the settlement case now runs in a child
process and the four router tests pass in shared-runner order. A fresh
combined build and exact-head CI/review remain required before final promotion.
[Sources, artifacts, limits and rollback](../validation/pr159-drawer-runtime-2026-11-13.md).

## PR 159 fetch worker continuation — 2026-11-13

File/data and disk-cache fetch I/O now run on bounded workers with owner-thread
completion. Fetchers share a network manager; corrupt/oversized cache recovery
and safe bounded redirects have regressions. Six Release native executables
and three ASan/UBSan image executables pass, as does the QNX cross-build and
Pi 400 resize/cache/ImageRef/blurhash check. The initial slice's independent
review has zero findings. Exact-head hosted checks remain required. The standalone image-cache link exposed a missing ImageDecode.cpp source;
that graph is corrected and its clean local loopback test passes. Explicit
cache API and ImageRef decode/hash/encode worker conversion remains open.
[Commands, evidence and rollback](../validation/pr159-image-io-2026-11-13.md).

## PR 159 image device continuation — 2026-11-13

The c2badc4 implementation head passed all hosted checks. On the custom-image
Pi 400, the new Zustand gallery driver shows a second image load after growth
from 20 to 240 pixels, successful cache/ImageRef/blurhash APIs, GIF frame
changes after start and stable pixels after stop. Window size remains 1280x800.
The loader I/O, cache recovery, redirects, navigation and wider conformance
work remains open. This driver needs its own CI and review.
[Evidence and rollback](../validation/pr159-device-image-2026-11-13.md).

## PR 159 image resolution continuation — 2026-11-13

Growing a downscaled still image now schedules an adequate decode, respecting
resource locks and decode options. All 30 native image-item host cases pass;
the growth and initial-decode race controls fail as expected. The review
finding is fixed; new-head CI and target
resize checks remain pending. Other image-loader/navigation gaps stay open.
[Receipt and rollback](../validation/pr159-image-resize-2026-11-13.md).

## PR 159 build repair — 2026-11-13

The missing command-handler initializer is fixed locally. Merge still requires the new head's required checks and review; the handoff's image-loader gaps remain open.
[Commands and limits](../validation/pr159-table-initializer-2026-11-13.md).

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
