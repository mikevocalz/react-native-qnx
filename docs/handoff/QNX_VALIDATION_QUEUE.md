# QNX Validation Queue

This queue separates host evidence from licensed QNX compilation and runtime
evidence. Host compilation against the test-only Screen subset must never be
reported as an SDP, ABI, driver, compositor, or hardware result.

## Allowed states

- Implemented and verified on macOS
- Implemented and verified on Linux
- Cross-compiled successfully for QNX
- Verified on a virtual QNX target
- Verified on physical QNX hardware
- Implemented but awaiting Linux validation
- Implemented but awaiting QNX target validation
- Partially implemented
- Blocked
- Not started

## QNX-GFX-001 — Screen context/window/buffer lifecycle

**Status:** Implemented but awaiting Linux validation

**Priority:** High

**Production files:**

- `native/platform/qnx/screen/QnxScreenSurface.h`
- `native/platform/qnx/screen/QnxScreenSurface.cpp`

**Current host evidence (2026-08-13):**

- The production translation unit compiles as C++20 with Apple Clang under
  `-Wall -Wextra -Wpedantic -Werror` against the isolated test-only QNX 8 Screen
  subset.
- ASan/UBSan host tests cover typed window creation, usage/format/size setup,
  double-buffer creation, exact cleanup order, idempotent destruction, move
  ownership, retry after failure, count mismatch, and every partial-initialize
  failure. Eight aggregate cases pass with zero fake live resources.
- This evidence does not validate installed QNX declarations or behavior.

**Installed-SDP source gate:**

Before compiling, cite the installed `screen/screen.h` declarations and constants
for every Screen call/property in the two production files. Verify the fake's
opaque handles, C signatures, and numeric constants against that installed
header. Stop and record an exact mismatch instead of adapting from memory.

**Required QNX build-host commands:**

```bash
./scripts/doctor.sh
./scripts/configure-qnx.sh aarch64 Debug
cmake --build --preset qnx-aarch64-debug --target qnx_platform
```

**Required target evidence:**

1. Context and typed application-window creation succeed.
2. Two render buffers are created with the selected format and dimensions.
3. Normal shutdown and every reproducible startup failure destroy all resources
   exactly once.
4. Repeated launches do not accumulate Screen resources.

**Advance state only when:** The installed-header audit and cross-compile output
are attached to `TEST_STATUS.md`; target evidence is required for any target
state.

## QNX-GFX-002 — ARGB fill, buffer acquisition, and presentation

**Status:** Implemented but awaiting Linux validation

**Priority:** High

**Production files:**

- `native/platform/qnx/screen/QnxScreenSurface.cpp`
- `native/platform/qnx/application/main.cpp`

**Current host evidence (2026-08-13):**

- The suite checks the exact signed ARGB `SCREEN_BLIT_COLOR` value and terminated
  token/value list passed to `screen_fill`.
- It checks that `fill()` and `present()` use the same buffer, and that the
  production code re-queries `SCREEN_PROPERTY_RENDER_BUFFERS` after every post
  rather than cycling stale handles.
- It checks that storage is available for both pointers returned by the
  double-buffered render-buffer property.
- The fake rotates its available-buffer list after a successful post; this is a
  behavioral test double, not a compositor model.

**Required QNX build-host commands:**

```bash
cmake --build --preset qnx-aarch64-debug --target rnqnx_playground
```

**Required target evidence:**

```bash
./scripts/deploy-qnx.sh aarch64 rnqnx_playground Debug
./scripts/run-qnx.sh aarch64 rnqnx_playground
```

Confirm the 1280×720 window becomes visible, alternates the intended ARGB blue
and black fills for up to 600 frames, posts without errors, and exits cleanly.
Record tearing, stale frames, format mismatches, and every returned `errno`.

## QNX-RN-008 — End-to-end Milestone 0 QNX validation

**Status:** Blocked

**Priority:** High

**Blocker:** No licensed QNX SDP 8.0.2 build host or QNX target is available in
the current workspace. See `BLOCKERS.md`.

**Required environment:**

- x86-64 Ubuntu 24.04 host
- licensed QNX SDP 8.0.2 with `QNX_HOST`/`QNX_TARGET` sourced
- installed target BSP, Screen headers, and `libscreen`
- AArch64 QNX target with Screen service and display

**Commands:**

```bash
pnpm install --frozen-lockfile
./scripts/doctor.sh
./scripts/configure-qnx.sh aarch64 Debug
./scripts/build-qnx.sh aarch64 Debug
export QNX_TARGET_IP=<target-ip>
./scripts/deploy-qnx.sh aarch64 rnqnx_playground Debug
./scripts/run-qnx.sh aarch64 rnqnx_playground
```

**Exit criteria:**

- `doctor.sh` exits zero.
- The installed Screen header/source gate for QNX-GFX-001 is complete.
- The build produces `build/qnx-aarch64-debug/bin/rnqnx_playground` with warnings
  as errors enabled.
- QNX-GFX-001 and QNX-GFX-002 reach a QNX target-verified state with recorded
  evidence.

## Queue discipline

Add an entry whenever a QNX-only source file is created or modified on a host.
Each entry must name the installed upstream source file/symbol that authorizes
the seam, the host checks performed, and the exact commands still required for
QNX compilation and runtime validation.
