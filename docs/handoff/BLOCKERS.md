# BLOCKERS.md

## QNX-RN-008 — Licensed QNX build and runtime environment unavailable

- **First observed:** 2026-07-19
- **Affected milestone:** Milestone 0 exit gate
- **Problem:** The current macOS arm64 workspace has no QNX SDP 8.0.5,
  `QNX_HOST`, `QNX_TARGET`, `qcc`/`q++`, installed Screen header/library, or QNX
  runtime target.
- **Reproduction:** Run `./scripts/configure-qnx.sh aarch64 Debug` without the
  SDP environment.
- **Current error:** CMake stops with `Environment variable QNX_HOST is not set`
  and instructions to source the SDP environment script.
- **Affected files:** `cmake/toolchains/qnx-8.0.cmake`, `CMakeLists.txt`,
  `native/platform/qnx/**`, and `scripts/{doctor,configure-qnx,build-qnx}.sh`.
- **Work completed:** The concrete Screen wrapper is compiled and tested on
  macOS against a test-only documented API subset with strict warnings,
  ASan, and UBSan. Failure cleanup and buffer sequencing are covered.
- **Why that is insufficient:** The fake cannot prove installed header
  signatures/constants, qcc compatibility, `libscreen` linkage, BSP behavior,
  compositor behavior, or target resource cleanup.
- **External prerequisite:** Licensed QNX SDP 8.0.5 and target BSP on an x86-64
  Ubuntu 24.04 host, plus a QNX target with Screen and a display.
- **Next investigation:** Follow QNX-RN-008 in `QNX_VALIDATION_QUEUE.md` and
  record the installed header/source symbols before changing any native seam.
- **Scope:** Blocks QNX Milestone 0 completion only; macOS host work remains
  available.

## Milestone 1 — Hermes QNX build recipe absent

- **First recorded:** 2026-08-13
- **Problem:** React Native 0.86.2 identifies Hermes
  `hermes-v250829098.0.16`, but the installed npm package contains host compiler
  binaries and Apple/Android build precedent only. It has no QNX recipe or full
  Hermes source checkout.
- **External/engineering prerequisite:** Acquire the exact upstream Hermes
  source tag, then validate OS, threads, ICU, memory mapping, and signal support
  with the installed QNX toolchain.
- **Scope:** Does not block the Screen-only Milestone 0 executable; blocks a
  truthful Milestone 1 Hermes runtime claim.

## UPSTREAM-DEP-001 — No published fix for Metro's image-size advisories

- **First observed:** 2026-08-13
- **Problem:** `pnpm audit --audit-level moderate` reports two high-severity
  infinite-loop denial-of-service advisories for `image-size` 1.2.1, resolved
  through Metro's development tooling.
- **Advisories:** GHSA-w3rx-r6r6-pgpr (ICNS) and GHSA-5p2g-fcmc-qvqq
  (JXL/HEIF).
- **Registry/source check:** On 2026-08-13 the registry's latest `image-size` is
  2.0.2 and no 2.0.3 package exists. Both advisories affect releases through
  2.0.2. Metro 0.84.4 and current Metro 0.87.0 both declare
  `image-size ^1.0.2`, so a compatible package refresh cannot remove the alert.
- **Current exposure:** Development dependency only; no Metro server or runnable
  application exists at Milestone 0. A crafted repository asset could hang a
  future Metro process.
- **Mitigation:** Do not process untrusted assets or run future Metro services on
  untrusted repositories. The vulnerable package is not shipped in a QNX binary
  at this milestone.
- **Resolution:** Monitor the upstream package/advisories. Adopt a fixed release
  only after Metro's import shape and malicious-buffer regression cases pass; do
  not silence the audit without a source-backed patch.
- **Scope:** Does not block host/C++ Milestone 0 work or the Screen-only QNX
  executable, but blocks calling the dependency audit clean and must be resolved
  before exposing a Metro development service.
