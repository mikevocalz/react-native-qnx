# NEXT_ACTIONS.md

## Next Action 1: Verify the build on an x86-64 Ubuntu 24.04 host with QNX SDP 8.0.5

**Objective:** Confirm that the Milestone 0 native executable configures, builds, and runs on a real QNX target.

**Files:**

- `cmake/toolchains/qnx-8.0.cmake`
- `CMakeLists.txt`
- `native/application/main.cpp`
- `native/screen/QnxScreenSurface.{h,cpp}`
- `scripts/doctor.sh`
- `scripts/configure-qnx.sh`
- `scripts/build-qnx.sh`

**Current behavior:**
The toolchain file correctly reports the missing QNX environment on a non-QNX host. It has not yet been exercised against a real QNX SDP 8.0.5 installation.

**Implementation:**

1. Install QNX SDP 8.0.5 and the target BSP on an x86-64 Ubuntu 24.04 host through QNX Software Center.
2. Source the SDP environment script.
3. Run `./scripts/doctor.sh` and resolve all reported errors.
4. Run `./scripts/configure-qnx.sh aarch64 Debug`.
5. Run `./scripts/build-qnx.sh aarch64 Debug`.
6. Deploy and run `rnqnx_playground` on a QNX target.

**Validation:**

- `./scripts/doctor.sh` exits 0.
- `build/qnx-aarch64-debug/bin/rnqnx_playground` is produced.
- On the target, a window opens and alternates color for 600 frames (20 seconds).

**Expected result:** End-to-end Milestone 0 exit gate satisfied.

**Acceptance criteria:**

- `rnqnx_playground` compiles with no warnings-as-errors.
- Binary runs on the QNX target.
- No memory/resource leaks detected on exit.

**Dependencies:** QNX SDP 8.0.5 license and installation; x86-64 Ubuntu 24.04 host; QNX AArch64 target.

**Known risks:**

- Exact `qcc`/`q++` target variant names may differ from the assumed `gcc_ntoaarch64le`/`gcc_ntox86_64`; the toolchain file must be adjusted if `qcc -V` shows different names.
- QNX Screen buffer API or supported pixel formats may vary by BSP.

---

## Next Action 2: Investigate Hermes cross-compilation for QNX

**Objective:** Determine the exact Hermes version bundled with React Native 0.86.0 and produce a QNX cross-compilation plan.

**Files:**

- `docs/getting-started/compatibility-matrix.md`
- `cmake/dependencies/Hermes.cmake` (to be created)
- `native/hermes/` (to be populated)

**Implementation:**

1. Identify the Hermes tag/commit used by React Native 0.86.0.
2. Evaluate whether Hermes can be built with `qcc`/`q++` for AArch64/x86-64 QNX.
3. Document required CMake flags, sysroot adjustments, and any QNX-specific patches.
4. Add a configure-time check for Hermes artifacts.

**Validation:**

- A written investigation note or ADR is added.
- If Hermes builds, `cmake/dependencies/Hermes.cmake` reports `HERMES_FOUND`.
- If blocked, a precise configure error with the missing component is produced.

**Expected result:** Clear path to Milestone 1 (Hermes and React runtime).

**Acceptance criteria:**

- No placeholder engine substitutions.
- Missing proprietary QNX components produce exact failure messages.

**Dependencies:** Next Action 1 (working QNX toolchain).

**Known risks:** Hermes ICU and Folly dependencies may require QNX-specific patches or additional sysroot libraries.

---

## Next Action 3: Set up CI foundation with self-hosted QNX runners

**Objective:** Provide automated build validation that does not expose QNX proprietary material.

**Files:**

- `.github/workflows/` or equivalent CI directory
- `scripts/ci-qnx-runner.sh`
- `docs/security/build-infra-security.md`

**Implementation:**

1. Add workflow that runs on self-hosted runner labels only when `QNX_HOST`/`QNX_TARGET` are present.
2. Run doctor, configure, and build steps.
3. Ensure no QNX license files or secrets are committed or logged.
4. Add artifact publishing for the native executable when build succeeds.

**Validation:**

- CI passes on a licensed self-hosted runner.
- CI skips or fails with a clear message on unsupported public runners.

**Expected result:** Reproducible automated builds for Milestone 0.

**Acceptance criteria:**

- CI does not download or redistribute QNX SDP.
- Build logs are sanitized before retention.

**Dependencies:** Next Action 1.

**Known risks:** Public GitHub runners cannot satisfy QNX licensing; only self-hosted runners are viable.
