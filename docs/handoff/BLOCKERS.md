# BLOCKERS.md

## 2026-11-10 — PR #102 readiness follow-up

Independent QNX/Qt review of the retained raw physical record and capture is still pending. Current-head host CI does not substitute for the skipped physical job. PR #110 owner feedback is a separate branch/report and does not promote this harness or the seven v1 release gates.

Receipt: [pi400-review-2026-11-10.md](../evidence/pi400-review-2026-11-10.md).

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
