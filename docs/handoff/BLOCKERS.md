# BLOCKERS.md

## QNX-RN-008 — Licensed QNX build and runtime environment unavailable

- **First observed:** 2026-07-19
- **Updated:** 2026-08-17
- **Affected milestone:** Milestone 0 exit gate
- **Problem (resolved):** A licensed QNX SDP 8.0.2 is now installed on this Windows
  x86-64 PC at `C:\Users\Owner\qnx800`, with `QNX_HOST`, `QNX_TARGET`, `qcc`,
  `q++`, the Screen headers, and `libscreen` available.
- **Problem (partially resolved):** A QNX x86_64 VM now boots on the same Windows
  PC under QEMU 11.1.0, with SSH access on `root@127.0.0.1:2222`. The G0 C++20
  probe and the Screen-only Milestone 0 executable `rnqnx_playground` were both
  cross-built for QNX x86_64, deployed, and run successfully inside the VM.
  `rnqnx_playground` initialized a 1280x720 Screen surface, rendered 600 frames,
  and exited cleanly using a software/dummy WFD configuration. See
  `docs/handoff/QNX_VM_WINDOWS.md` for the exact workaround and reproduction
  steps.
- **Problem (remaining):** No physical QNX AArch64 runtime target with a real
  display/GPU and a named SSH user/key is configured. The x86_64 QEMU VM uses a
  software/dummy Screen blitter, so it validates the code path and resource
  lifecycle but not an actual GPU/display driver or the AArch64le build.
- **Reproduction:** Previously `./scripts/configure-qnx.sh aarch64 Debug` failed
  with `Environment variable QNX_HOST is not set`. It now configures and builds.
- **Current status:**
  - `cmake --fresh --preset qnx-aarch64-debug` succeeds.
  - `cmake --build --preset qnx-aarch64-debug` produces `rnqnx_playground`
    linked against `libscreen.so.1` and the `libqnx_platform.a` static library
    for AArch64 QNX.
  - `cmake --build --preset qnx-pi4` produces `rnqnx_g0_cxx20_probe` for
    AArch64 QNX; this SDP's libc++ lacks `std::jthread`, so the probe falls back
    to `std::thread` and reports `jthread=unsupported`.
  - `cmake --build --preset qnx-x86_64-debug` produces `rnqnx_g0_cxx20_probe` for
    QNX x86_64, which runs inside the QEMU VM described above.
- **Affected files:** `vnext/cmake/qnx-aarch64le.toolchain.cmake`,
  `CMakeLists.txt`, `vnext/probes/cxx20/main.cpp`, `scripts/validate-g0-a.sh`,
  `native/platform/qnx/**`, and `scripts/{doctor,configure-qnx,build-qnx}.sh`.
- **External prerequisite:** A QNX AArch64 target with Screen/display and a named
  SSH user/key (not root). Set `QNX_TARGET_IP` and `QNX_TARGET_USER` to run
  `scripts/validate-g0-a.sh` and `scripts/deploy-qnx.sh`.
- **Next investigation:** Deploy and run `rnqnx_playground` and
  `rnqnx_g0_cxx20_probe` on a QNX AArch64 target with Screen; record
  `slog2info` and stdout. Until that hardware is available, the x86_64 VM can
  exercise non-Screen binaries and the deploy/SSH loop.
- **Scope:** Blocks QNX Milestone 0 exit until AArch64+Screen target evidence is
  captured; host and cross-compile work is unblocked.

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
